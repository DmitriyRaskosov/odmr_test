#include "packet_collector/fpga2.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void usage(const char* prog) {
    fprintf(stderr,
            "Usage: %s <iface> [options]\n"
            "Options:\n"
            "  --analyze-stream     aggregate ch0/ch2 on the fly (no raw txt by default)\n"
            "  --record-raw         also write ch*_*.txt (debug)\n"
            "  --output-dir DIR     directory for all output files (created if missing)\n"
            "  -o, --output FILE    analyze output (default: pulses_grouped.txt)\n"
            "  --group-size N       pulses per even/odd group (default: 400)\n"
            "  --photon-channel N   photon channel id (default: 0)\n"
            "  --trigger-channel N  trigger channel id (default: 2)\n",
            prog);
}

static void init_capture_config(CaptureConfig* config) {
    memset(config, 0, sizeof(*config));
    config->record_raw = 1;
    config->analyze_stream = 0;
    config->output_path = "pulses_grouped.txt";
    config->group_size = 400;
    config->photon_channel = 0;
    config->trigger_channel = 2;
}

int main(int argc, char **argv) {
    CaptureConfig config;
    init_capture_config(&config);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--analyze-stream") == 0) {
            config.analyze_stream = 1;
            config.record_raw = 0;
        } else if (strcmp(argv[i], "--record-raw") == 0) {
            config.record_raw = 1;
        } else if (strcmp(argv[i], "--output-dir") == 0) {
            if (i + 1 >= argc) {
                usage(argv[0]);
                return 1;
            }
            config.output_dir = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                usage(argv[0]);
                return 1;
            }
            config.output_path = argv[++i];
        } else if (strcmp(argv[i], "--group-size") == 0) {
            if (i + 1 >= argc) {
                usage(argv[0]);
                return 1;
            }
            config.group_size = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--photon-channel") == 0) {
            if (i + 1 >= argc) {
                usage(argv[0]);
                return 1;
            }
            config.photon_channel = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--trigger-channel") == 0) {
            if (i + 1 >= argc) {
                usage(argv[0]);
                return 1;
            }
            config.trigger_channel = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        } else if (!config.iface) {
            config.iface = argv[i];
        } else {
            fprintf(stderr, "Unexpected argument: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (!config.iface) {
        usage(argv[0]);
        return 1;
    }

    fprintf(stderr,
            "packet_capture: iface=%s analyze=%d record_raw=%d output_dir=%s (Ctrl+C to stop)\n",
            config.iface,
            config.analyze_stream,
            config.record_raw,
            config.output_dir ? config.output_dir : ".");
    fflush(stderr);
    return start_capture_config(&config);
}
