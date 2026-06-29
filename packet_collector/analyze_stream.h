#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* output_path;
    /** Even/odd pulse pairs per sweep point (one output group). */
    int repeats_per_freq;
    int photon_channel;
    int trigger_channel;
    /** Expected output groups from Rigol sweep (0 = unknown). */
    int expected_groups;
} AnalyzeStreamConfig;

typedef struct AnalyzeStream AnalyzeStream;

AnalyzeStream* analyze_stream_create(const AnalyzeStreamConfig* config);
void analyze_stream_destroy(AnalyzeStream* stream);

void analyze_stream_feed(
    AnalyzeStream* stream,
    int channel,
    const double* timestamps,
    const int* fronts,
    const uint32_t* raw_words,
    int count,
    int* ms_offset
);

void analyze_stream_finish(AnalyzeStream* stream);

unsigned long analyze_stream_groups_written(const AnalyzeStream* stream);
int analyze_stream_expected_groups(const AnalyzeStream* stream);
/** Non-zero when expected_groups > 0 and all groups have been written. */
int analyze_stream_is_complete(const AnalyzeStream* stream);
unsigned long analyze_stream_pulses_completed(const AnalyzeStream* stream);

size_t analyze_stream_photon_buffer_count(const AnalyzeStream* stream);
size_t analyze_stream_photon_buffer_peak(const AnalyzeStream* stream);
unsigned long analyze_stream_photons_trimmed(const AnalyzeStream* stream);
unsigned long analyze_stream_bad_windows_skipped(const AnalyzeStream* stream);
unsigned long analyze_stream_implicit_markers(const AnalyzeStream* stream);

#ifdef __cplusplus
}
#endif
