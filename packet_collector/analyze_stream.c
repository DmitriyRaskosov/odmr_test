#include "analyze_stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHOTON_GROW 4096
#define PULSE_GROW 256

struct AnalyzeStream {
    AnalyzeStreamConfig config;
    FILE* output;
    unsigned long groups_written;
    unsigned long pulses_completed;

    double* photons;
    size_t photon_count;
    size_t photon_capacity;
    size_t photon_buffer_peak;
    unsigned long photons_trimmed_total;

    unsigned long* pulse_counts;
    size_t pulse_count;
    size_t pulse_capacity;

    double pending_trigger_start;
    int has_pending_trigger_start;
    unsigned long bad_windows_skipped;
    unsigned long implicit_markers;

    double last_raw_ms[4];
    int have_last_raw[4];
};

static size_t bisect_left(const double* values, size_t begin, size_t count, double value) {
    size_t lo = begin;
    size_t hi = begin + count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (values[mid] < value) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

static void track_photon_buffer_peak(AnalyzeStream* stream) {
    if (stream->photon_count > stream->photon_buffer_peak) {
        stream->photon_buffer_peak = stream->photon_count;
    }
}

static void trim_photons_before_index(AnalyzeStream* stream, size_t keep_from) {
    if (keep_from == 0 || stream->photon_count == 0) {
        return;
    }
    if (keep_from >= stream->photon_count) {
        stream->photons_trimmed_total += (unsigned long)stream->photon_count;
        stream->photon_count = 0;
        return;
    }

    size_t remaining = stream->photon_count - keep_from;
    memmove(stream->photons, stream->photons + keep_from, remaining * sizeof(double));
    stream->photon_count = remaining;
    stream->photons_trimmed_total += (unsigned long)keep_from;
}

static int ensure_photon_capacity(AnalyzeStream* stream, size_t needed) {
    if (needed <= stream->photon_capacity) {
        return 1;
    }
    size_t new_cap = stream->photon_capacity ? stream->photon_capacity : PHOTON_GROW;
    while (new_cap < needed) {
        new_cap += PHOTON_GROW;
    }
    double* grown = (double*)realloc(stream->photons, new_cap * sizeof(double));
    if (!grown) {
        return 0;
    }
    stream->photons = grown;
    stream->photon_capacity = new_cap;
    return 1;
}

static int ensure_pulse_capacity(AnalyzeStream* stream, size_t needed) {
    if (needed <= stream->pulse_capacity) {
        return 1;
    }
    size_t new_cap = stream->pulse_capacity ? stream->pulse_capacity : PULSE_GROW;
    while (new_cap < needed) {
        new_cap += PULSE_GROW;
    }
    unsigned long* grown = (unsigned long*)realloc(stream->pulse_counts, new_cap * sizeof(unsigned long));
    if (!grown) {
        return 0;
    }
    stream->pulse_counts = grown;
    stream->pulse_capacity = new_cap;
    return 1;
}

static void append_photon(AnalyzeStream* stream, double value) {
    if (!ensure_photon_capacity(stream, stream->photon_count + 1)) {
        fprintf(stderr, "analyze_stream: out of memory for photon buffer\n");
        return;
    }
    stream->photons[stream->photon_count++] = value;
    track_photon_buffer_peak(stream);
}

static void complete_pulse(AnalyzeStream* stream, double start, double end) {
    unsigned long count = 0;

    if (stream->photon_count > 0) {
        size_t left = bisect_left(stream->photons, 0, stream->photon_count, start);
        size_t right = bisect_left(stream->photons, 0, stream->photon_count, end);
        if (right > left) {
            count = (unsigned long)(right - left);
        }
        trim_photons_before_index(stream, right);
    }

    if (!ensure_pulse_capacity(stream, stream->pulse_count + 1)) {
        fprintf(stderr, "analyze_stream: out of memory for pulse buffer\n");
        return;
    }
    stream->pulse_counts[stream->pulse_count++] = count;
    stream->pulses_completed++;
}

static void write_group_row(
    AnalyzeStream* stream,
    unsigned long even_sum,
    unsigned long odd_sum
) {
    if (!stream->output) {
        return;
    }

    fprintf(stream->output, "%lu, %lu, %lu\n", stream->groups_written, even_sum, odd_sum);
    fflush(stream->output);
    stream->groups_written++;
}

static void write_group_if_ready(AnalyzeStream* stream) {
    int n = stream->config.repeats_per_freq;
    if (n <= 0 || !stream->output) {
        return;
    }

    unsigned long pairs_available = stream->pulse_count / 2;
    unsigned long groups_available = pairs_available / (unsigned long)n;
    if (groups_available == 0) {
        return;
    }

    size_t offset = 0;

    while (offset + (size_t)n * 2 <= stream->pulse_count) {
        if (stream->config.expected_groups > 0 &&
            (int)stream->groups_written >= stream->config.expected_groups) {
            fprintf(stderr,
                    "analyze_stream: warning: extra sweep data after group %d "
                    "(expected_groups=%d)\n",
                    stream->config.expected_groups - 1,
                    stream->config.expected_groups);
            break;
        }

        unsigned long even_sum = 0;
        unsigned long odd_sum = 0;

        for (int i = 0; i < n; i++) {
            size_t even_idx = offset + (size_t)i * 2;
            size_t odd_idx = even_idx + 1;
            even_sum += stream->pulse_counts[even_idx];
            odd_sum += stream->pulse_counts[odd_idx];
        }

        write_group_row(stream, even_sum, odd_sum);
        offset += (size_t)n * 2;
    }

    if (offset > 0) {
        size_t remaining = stream->pulse_count - offset;
        memmove(stream->pulse_counts, stream->pulse_counts + offset, remaining * sizeof(unsigned long));
        stream->pulse_count = remaining;
    }
}

static void validate_group_count(AnalyzeStream* stream) {
    if (!stream || stream->config.expected_groups <= 0) {
        return;
    }

    int expected = stream->config.expected_groups;
    unsigned long written = stream->groups_written;

    if ((int)written == expected) {
        fprintf(stderr,
                "analyze_stream: groups written %lu / expected %d (OK)\n",
                written,
                expected);
        return;
    }

    fprintf(stderr,
            "analyze_stream: WARNING groups written %lu, expected %d "
            "(repeats_per_freq=%d)\n",
            written,
            expected,
            stream->config.repeats_per_freq);
}

static void append_trigger(AnalyzeStream* stream, double value, int ms_offset) {
    if (!stream->has_pending_trigger_start) {
        stream->pending_trigger_start = value;
        stream->has_pending_trigger_start = 1;
        return;
    }

    if (value <= stream->pending_trigger_start) {
        static int bad_window_logs = 0;
        stream->bad_windows_skipped++;
        if (bad_window_logs < 3) {
            fprintf(stderr,
                    "analyze_stream: bad pulse window start=%.9f end=%.9f ms_offset=%d (skipped)\n",
                    stream->pending_trigger_start,
                    value,
                    ms_offset);
            bad_window_logs++;
        }
        stream->has_pending_trigger_start = 0;
        return;
    }

    complete_pulse(stream, stream->pending_trigger_start, value);
    stream->has_pending_trigger_start = 0;
    write_group_if_ready(stream);
}

AnalyzeStream* analyze_stream_create(const AnalyzeStreamConfig* config) {
    if (!config || !config->output_path) {
        return NULL;
    }

    AnalyzeStream* stream = (AnalyzeStream*)calloc(1, sizeof(AnalyzeStream));
    if (!stream) {
        return NULL;
    }

    stream->config = *config;
    if (stream->config.repeats_per_freq <= 0) {
        stream->config.repeats_per_freq = 1000;
    }

    stream->output = fopen(config->output_path, "w");
    if (!stream->output) {
        fprintf(stderr, "analyze_stream: cannot open %s\n", config->output_path);
        free(stream);
        return NULL;
    }

    fprintf(stream->output, "# group, even_photons, odd_photons\n");
    fflush(stream->output);
    return stream;
}

void analyze_stream_destroy(AnalyzeStream* stream) {
    if (!stream) {
        return;
    }
    if (stream->output) {
        fclose(stream->output);
    }
    free(stream->photons);
    free(stream->pulse_counts);
    free(stream);
}

void analyze_stream_feed(
    AnalyzeStream* stream,
    int channel,
    const double* timestamps,
    const int* fronts,
    const uint32_t* raw_words,
    int count,
    int* ms_offset
) {
    (void)fronts;

    if (!stream || !timestamps || !raw_words || !ms_offset || count <= 0) {
        return;
    }
    if (channel < 0 || channel >= 4) {
        return;
    }

    int current_offset = *ms_offset;

    for (int i = 0; i < count; i++) {
        if (raw_words[i] == 0u) {
            current_offset++;
            stream->have_last_raw[channel] = 0;
            continue;
        }

        if (stream->have_last_raw[channel] &&
            timestamps[i] < stream->last_raw_ms[channel]) {
            current_offset++;
            stream->implicit_markers++;
        }

        double corrected = 100.0 * current_offset + timestamps[i];

        if (channel == stream->config.trigger_channel &&
            stream->has_pending_trigger_start &&
            corrected <= stream->pending_trigger_start) {
            int guard = 0;
            while (corrected <= stream->pending_trigger_start && guard < 16) {
                current_offset++;
                corrected = 100.0 * current_offset + timestamps[i];
                stream->implicit_markers++;
                guard++;
            }
        }

        if (channel == stream->config.photon_channel) {
            append_photon(stream, corrected);
        } else if (channel == stream->config.trigger_channel) {
            append_trigger(stream, corrected, current_offset);
        }

        stream->last_raw_ms[channel] = timestamps[i];
        stream->have_last_raw[channel] = 1;
    }

    *ms_offset = current_offset;
}

void analyze_stream_finish(AnalyzeStream* stream) {
    if (!stream) {
        return;
    }
    if (stream->has_pending_trigger_start) {
        fprintf(stderr, "analyze_stream: warning: odd trigger count, dropping last edge\n");
        stream->has_pending_trigger_start = 0;
    }
    write_group_if_ready(stream);
    validate_group_count(stream);
    if (stream->output) {
        fflush(stream->output);
    }
}

unsigned long analyze_stream_groups_written(const AnalyzeStream* stream) {
    return stream ? stream->groups_written : 0;
}

int analyze_stream_expected_groups(const AnalyzeStream* stream) {
    return stream ? stream->config.expected_groups : 0;
}

int analyze_stream_is_complete(const AnalyzeStream* stream) {
    if (!stream || stream->config.expected_groups <= 0) {
        return 0;
    }
    return (int)stream->groups_written >= stream->config.expected_groups;
}

unsigned long analyze_stream_pulses_completed(const AnalyzeStream* stream) {
    return stream ? stream->pulses_completed : 0;
}

size_t analyze_stream_photon_buffer_count(const AnalyzeStream* stream) {
    return stream ? stream->photon_count : 0;
}

size_t analyze_stream_photon_buffer_peak(const AnalyzeStream* stream) {
    return stream ? stream->photon_buffer_peak : 0;
}

unsigned long analyze_stream_photons_trimmed(const AnalyzeStream* stream) {
    return stream ? stream->photons_trimmed_total : 0;
}

unsigned long analyze_stream_bad_windows_skipped(const AnalyzeStream* stream) {
    return stream ? stream->bad_windows_skipped : 0;
}

unsigned long analyze_stream_implicit_markers(const AnalyzeStream* stream) {
    return stream ? stream->implicit_markers : 0;
}
