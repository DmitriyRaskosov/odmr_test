#pragma once
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* iface;
    int record_raw;
    int analyze_stream;
    const char* output_dir;
    const char* output_path;
    /** Even/odd pulse pairs per Rigol frequency (--group-size alias). */
    int repeats_per_freq;
    int photon_channel;
    int trigger_channel;
    /** Expected groups from sweep: round((stop-start)/step)+1; 0 = unknown. */
    int expected_groups;
    const char* experiment_ini;
} CaptureConfig;

int start_capture(int argc, char **argv);
int start_capture_config(const CaptureConfig* config);

extern volatile int keep_running;

#ifdef __cplusplus
}
#endif
