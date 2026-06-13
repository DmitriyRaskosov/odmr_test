#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* output_path;
    int group_size;
    int photon_channel;
    int trigger_channel;
} AnalyzeStreamConfig;

typedef struct AnalyzeStream AnalyzeStream;

AnalyzeStream* analyze_stream_create(const AnalyzeStreamConfig* config);
void analyze_stream_destroy(AnalyzeStream* stream);

/* Feed decoded events from one UDP payload (after +100ms correction). */
void analyze_stream_feed(
    AnalyzeStream* stream,
    int channel,
    const double* timestamps,
    const int* fronts,
    const uint32_t* raw_words,
    int count,
    int* ms_offset
);

/* Flush pending groups and close output file. */
void analyze_stream_finish(AnalyzeStream* stream);

unsigned long analyze_stream_groups_written(const AnalyzeStream* stream);
unsigned long analyze_stream_pulses_completed(const AnalyzeStream* stream);

size_t analyze_stream_photon_buffer_count(const AnalyzeStream* stream);
size_t analyze_stream_photon_buffer_peak(const AnalyzeStream* stream);
unsigned long analyze_stream_photons_trimmed(const AnalyzeStream* stream);
unsigned long analyze_stream_bad_windows_skipped(const AnalyzeStream* stream);

#ifdef __cplusplus
}
#endif
