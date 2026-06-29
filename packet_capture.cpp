#include "packet_collector/fpga2.h"
#include "config_parser.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

static void usage(const char* prog) {
    fprintf(stderr,
            "Usage: %s <iface> [options]\n"
            "Options:\n"
            "  --analyze-stream       aggregate ch0/ch2 on the fly (no raw txt by default)\n"
            "  --record-raw           also write ch*_*.txt (debug)\n"
            "  --output-dir DIR       directory for all output files (created if missing)\n"
            "  -o, --output FILE      analyze output (default: pulses_grouped.txt)\n"
            "  --experiment-ini FILE  cv_odmr.ini: repeats + expected group count from sweep\n"
            "  --repeats-per-freq N   even+odd pairs per group (from ini if omitted)\n"
            "  --expected-groups N    output rows expected (from ini sweep if omitted)\n"
            "  --group-size N         alias for --repeats-per-freq (legacy)\n"
            "  --photon-channel N     photon channel id (default: 0)\n"
            "  --trigger-channel N    trigger channel id (default: 2)\n",
            prog);
}

static void init_capture_config(CaptureConfig* config) {
    memset(config, 0, sizeof(*config));
    config->record_raw = 1;
    config->analyze_stream = 0;
    config->output_path = "pulses_grouped.txt";
    config->repeats_per_freq = 0;
    config->photon_channel = 0;
    config->trigger_channel = 2;
    config->expected_groups = 0;
}

static int compute_expected_groups(double start_hz, double stop_hz, double step_hz) {
    if (step_hz <= 0.0 || stop_hz <= start_hz) {
        return 0;
    }
    int points = static_cast<int>(std::round((stop_hz - start_hz) / step_hz)) + 1;
    return points > 0 ? points : 0;
}

static void apply_experiment_ini(CaptureConfig* config) {
    if (!config->experiment_ini || !config->experiment_ini[0]) {
        return;
    }

    try {
        ConfigParser parser(config->experiment_ini);
        CV_config cv = parser.parse_cv_config();
        Rigol_config_cv rigol = parser.parse_rigol_config_cv();

        if (config->repeats_per_freq <= 0 && cv.num_repeats > 0) {
            config->repeats_per_freq = cv.num_repeats;
        }
        if (config->expected_groups <= 0 && rigol.start_freq > 0.0 && rigol.stop_freq > rigol.start_freq &&
            rigol.freq_step > 0.0) {
            config->expected_groups = compute_expected_groups(
                rigol.start_freq,
                rigol.stop_freq,
                rigol.freq_step);
        }

        fprintf(stderr,
                "packet_capture: experiment ini %s -> repeats_per_freq=%d expected_groups=%d\n",
                config->experiment_ini,
                config->repeats_per_freq,
                config->expected_groups);
    } catch (const std::exception& ex) {
        fprintf(stderr, "packet_capture: failed to read %s: %s\n",
                config->experiment_ini, ex.what());
    }
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
        } else if (strcmp(argv[i], "--experiment-ini") == 0) {
            if (i + 1 >= argc) {
                usage(argv[0]);
                return 1;
            }
            config.experiment_ini = argv[++i];
        } else if (strcmp(argv[i], "--repeats-per-freq") == 0 ||
                   strcmp(argv[i], "--group-size") == 0) {
            if (i + 1 >= argc) {
                usage(argv[0]);
                return 1;
            }
            config.repeats_per_freq = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--expected-groups") == 0) {
            if (i + 1 >= argc) {
                usage(argv[0]);
                return 1;
            }
            config.expected_groups = atoi(argv[++i]);
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

    if (config.experiment_ini) {
        apply_experiment_ini(&config);
    }

    if (config.repeats_per_freq <= 0) {
        config.repeats_per_freq = 1000;
    }

    fprintf(stderr,
            "packet_capture: iface=%s analyze=%d record_raw=%d output_dir=%s "
            "repeats_per_freq=%d expected_groups=%d (Ctrl+C to stop)\n",
            config.iface,
            config.analyze_stream,
            config.record_raw,
            config.output_dir ? config.output_dir : ".",
            config.repeats_per_freq,
            config.expected_groups);
    fflush(stderr);
    return start_capture_config(&config);
}
