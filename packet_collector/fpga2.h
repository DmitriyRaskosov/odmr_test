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
    int group_size;
    int photon_channel;
    int trigger_channel;
} CaptureConfig;

int start_capture(int argc, char **argv);
int start_capture_config(const CaptureConfig* config);

extern volatile int keep_running;

#ifdef __cplusplus
}
#endif
