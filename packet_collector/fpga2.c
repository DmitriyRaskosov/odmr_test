#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <poll.h>
#include <signal.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include "fpga2.h"
#include "analyze_stream.h"
#include "channel_config.h"
#include "packet_reorder.h"
#define K (long)1024
#define M (long)1024*K

#define QUEUE_SIZE           20000
#define KERNEL_STATS_EVERY   8192u
#define PERIODIC_STATS_EVERY 32768u
#define FLUSH_EVERY_PACKETS  512u
#define LOG_EVERY_FAILURES   1000u

static const char* filenames[] = {"ch0_", "ch1_", "ch2_", "ch3_"};
static FILE* files[MAX_CHANNELS] = {NULL, NULL, NULL, NULL};

static CaptureConfig g_capture = {0};
static AnalyzeStream* g_analyzer = NULL;
static char g_output_dir[PATH_MAX] = "";
static char g_analyze_output_path[PATH_MAX] = "";

#define BLOCK_SIZE      (1<<20)
#define BLOCK_NR        128
#define FRAME_SIZE      2048
#define FRAME_NR        ((BLOCK_SIZE * BLOCK_NR) / FRAME_SIZE)

volatile int keep_running = 1;
static volatile int capture_finished = 0;

typedef enum channel {
    UNDEFINED = -1,
    CH_0 = 0,
    CH_1 = 1,
    CH_2 = 2,
    CH_3 = 3
} Channel_t;

static Channel_t packet_channel(const unsigned char* packet) {
    unsigned int num = packet[0x2a] & 0x0F;
    if (num >= MAX_CHANNELS) {
        return UNDEFINED;
    }
    return (Channel_t)num;
}

static uint16_t packet_payload_counter(const unsigned char* packet) {
    return (uint16_t)(((unsigned)packet[0x2c] << 8) | packet[0x2d]);
}

typedef struct {
    unsigned char* data;
    int len;
    Channel_t channel;
} packet_info_t;

static packet_info_t queue[QUEUE_SIZE];
static volatile int queue_head = 0;
static volatile int queue_tail = 0;
static volatile int queue_count = 0;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;

static volatile unsigned long stat_packets_enqueued = 0;
static volatile unsigned long stat_enqueue_fail = 0;
static volatile unsigned long stat_wrong_len = 0;
static volatile unsigned long stat_write_errors = 0;
static volatile unsigned long stat_markers_100ms = 0;
static volatile unsigned long stat_kernel_drops_total = 0;
static unsigned long stat_reorder_late_drops = 0;
static unsigned long stat_reorder_pending_peak = 0;
static unsigned long stat_reorder_overflow = 0;

static void stat_inc(volatile unsigned long* counter) {
    __sync_fetch_and_add(counter, 1);
}

static void note_enqueue_failure(void) {
    unsigned long n = __sync_fetch_and_add(&stat_enqueue_fail, 1) + 1;
    if (n <= 5 || (n % LOG_EVERY_FAILURES) == 0) {
        fprintf(stderr, "packet_collector: enqueue failed (out of memory?), total=%lu\n", n);
    }
}

static void note_write_error(Channel_t ch) {
    unsigned long n = __sync_fetch_and_add(&stat_write_errors, 1) + 1;
    if (n <= 5 || (n % LOG_EVERY_FAILURES) == 0) {
        fprintf(stderr, "packet_collector: write error on channel %d, total=%lu\n", ch, n);
    }
}

static void report_kernel_drops(int sock) {
    struct tpacket_stats stats;
    socklen_t slen = sizeof(stats);

    if (getsockopt(sock, SOL_PACKET, PACKET_STATISTICS, &stats, &slen) != 0) {
        return;
    }
    if (stats.tp_drops == 0) {
        return;
    }

    unsigned long total = __sync_fetch_and_add(&stat_kernel_drops_total, stats.tp_drops) + stats.tp_drops;
    fprintf(stderr, "packet_collector: kernel drops +%u (session total=%lu)\n", stats.tp_drops, total);
}

static void print_periodic_stats(int sock, unsigned long queue_depth) {
    fprintf(stderr,
            "packet_collector: enqueued=%lu enqueue_fail=%lu wrong_len=%lu "
            "write_err=%lu kernel_drops=%lu queue=%lu\n",
            stat_packets_enqueued,
            stat_enqueue_fail,
            stat_wrong_len,
            stat_write_errors,
            stat_kernel_drops_total,
            queue_depth);
    fflush(stderr);
}

static int ensure_output_dir(const char* dir) {
    if (!dir || !dir[0]) {
        return 0;
    }

    char tmp[PATH_MAX];
    if (snprintf(tmp, sizeof(tmp), "%s", dir) >= (int)sizeof(tmp)) {
        fprintf(stderr, "packet_collector: output dir path too long\n");
        return -1;
    }

    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (char* p = tmp + 1; *p; p++) {
        if (*p != '/') {
            continue;
        }
        *p = '\0';
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            perror(tmp);
            return -1;
        }
        *p = '/';
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        perror(tmp);
        return -1;
    }
    return 0;
}

static const char* resolve_analyze_output_path(const CaptureConfig* config) {
    const char* output_path = config->output_path ? config->output_path : "pulses_grouped.txt";

    if (!config->output_dir || !config->output_dir[0]) {
        if (snprintf(g_analyze_output_path, sizeof(g_analyze_output_path), "%s", output_path)
            >= (int)sizeof(g_analyze_output_path)) {
            return NULL;
        }
        return g_analyze_output_path;
    }

    if (output_path[0] == '/') {
        if (snprintf(g_analyze_output_path, sizeof(g_analyze_output_path), "%s", output_path)
            >= (int)sizeof(g_analyze_output_path)) {
            return NULL;
        }
        return g_analyze_output_path;
    }

    const char* leaf = strrchr(output_path, '/');
    leaf = leaf ? leaf + 1 : output_path;
    if (snprintf(g_analyze_output_path, sizeof(g_analyze_output_path), "%s/%s", config->output_dir, leaf)
        >= (int)sizeof(g_analyze_output_path)) {
        return NULL;
    }
    return g_analyze_output_path;
}

static void print_capture_summary(int sock) {
    struct tpacket_stats stats;
    socklen_t slen = sizeof(stats);

    if (getsockopt(sock, SOL_PACKET, PACKET_STATISTICS, &stats, &slen) == 0 && stats.tp_drops > 0) {
        stat_kernel_drops_total += stats.tp_drops;
    }

    fprintf(stderr,
            "packet_collector summary:\n"
            "  packets enqueued:   %lu\n"
            "  enqueue failures:   %lu\n"
            "  wrong frame length: %lu\n"
            "  write errors:       %lu\n"
            "  +100ms markers:     %lu\n"
            "  kernel drops total: %lu\n",
            stat_packets_enqueued,
            stat_enqueue_fail,
            stat_wrong_len,
            stat_write_errors,
            stat_markers_100ms,
            stat_kernel_drops_total);
    if (g_analyzer) {
        fprintf(stderr,
                "  reorder pending peak: %lu\n"
                "  reorder late drops:   %lu\n"
                "  reorder overflow:     %lu\n",
                stat_reorder_pending_peak,
                stat_reorder_late_drops,
                stat_reorder_overflow);
        fprintf(stderr,
                "  analyze output:     %s\n"
                "  analyze groups:     %lu\n"
                "  analyze pulses:     %lu\n"
                "  photon buffer:      %zu (peak %zu)\n"
                "  photons trimmed:    %lu\n"
                "  bad pulse windows:  %lu (skipped)\n",
                g_analyze_output_path[0] ? g_analyze_output_path : "(unknown)",
                analyze_stream_groups_written(g_analyzer),
                analyze_stream_pulses_completed(g_analyzer),
                analyze_stream_photon_buffer_count(g_analyzer),
                analyze_stream_photon_buffer_peak(g_analyzer),
                analyze_stream_photons_trimmed(g_analyzer),
                analyze_stream_bad_windows_skipped(g_analyzer));
    }
    fflush(stderr);
}

void int_handler(int sig) {
    (void)sig;
    keep_running = 0;
}

void open_file(Channel_t ch) {
    static int counters[MAX_CHANNELS] = {0, 0, 0, 0};
    if (ch < 0 || ch >= MAX_CHANNELS) {
        return;
    }
    char fname[PATH_MAX];
    if (g_output_dir[0]) {
        snprintf(fname, sizeof(fname), "%s/%s%d.txt", g_output_dir, filenames[ch], counters[ch]);
    } else {
        snprintf(fname, sizeof(fname), "%s%d.txt", filenames[ch], counters[ch]);
    }
    FILE *log = fopen(fname, "w");
    counters[ch]++;
    if (!log) {
        fprintf(stderr, "packet_collector: cannot open %s\n", fname);
        return;
    }
    setvbuf(log, NULL, _IOFBF, 256 * 1024);
    files[ch] = log;
}

void parse_timestamps_raw(
    const unsigned char* packet,
    double* timestamps,
    int* fronts,
    uint32_t* raw_words,
    int* count,
    Channel_t ch
) {
    size_t index = 0x2a + 4;
    int cnt = 0;

    while (index + 3 < 1066 && cnt < 255) {
        uint32_t timestamp = ((uint32_t)packet[index] << 24) |
                             ((uint32_t)packet[index + 1] << 16) |
                             ((uint32_t)packet[index + 2] << 8) |
                             (uint32_t)packet[index + 3];

        if (raw_words) {
            raw_words[cnt] = timestamp;
        }

        if (ch == CH_0 || ch == CH_1 || ch == CH_3) {
            int nsec = ((timestamp & 0xFFFFFFC0) >> 6) * 5;
            int pics = (timestamp & 0x0000001F) * 185;
            timestamps[cnt] = (double)nsec / 1e6 + (double)pics / 1e9;
            fronts[cnt] = -1;
        } else {
            int nsec = ((timestamp & 0xFFFFFFC0) >> 6) * 5;
            timestamps[cnt] = (double)nsec / 1e6;
            fronts[cnt] = (timestamp & 0x00000004) ? 1 : 0;
        }
        cnt++;
        index += 4;
    }
    *count = cnt;
}

static int write_timestamp_lines(
    FILE* out,
    Channel_t ch,
    const double* raw_timestamps,
    const int* fronts,
    const uint32_t* raw_words,
    int ts_count,
    int* ms_offset
) {
    int current_offset = *ms_offset;
    int lines_written = 0;

    for (int i = 0; i < ts_count; i++) {
        if (raw_words[i] == 0u) {
            current_offset++;
            continue;
        }

        double corrected = 100.0 * current_offset + raw_timestamps[i];
        int rc;
        if (ch == CH_0 || ch == CH_1 || ch == CH_3) {
            rc = fprintf(out, "%.7f\n", corrected);
        } else {
            rc = fprintf(out, "%.6f%d\n", corrected, fronts[i]);
        }
        if (rc < 0) {
            note_write_error(ch);
            break;
        }
        lines_written++;
    }

    *ms_offset = current_offset;
    return lines_written;
}

typedef struct {
    int ms_offsets_analyze[MAX_CHANNELS];
    int ms_offsets_raw[MAX_CHANNELS];
    unsigned long packets_since_flush[MAX_CHANNELS];
} write_thread_ctx_t;

static void process_ordered_packet(void* ctx, int channel, const unsigned char* data, int len) {
    write_thread_ctx_t* wctx = (write_thread_ctx_t*)ctx;

    if (channel < 0 || channel >= MAX_CHANNELS) {
        return;
    }

    double raw_timestamps[256];
    int fronts[256];
    uint32_t raw_words[256];
    int ts_count = 0;
    parse_timestamps_raw(data, raw_timestamps, fronts, raw_words, &ts_count, (Channel_t)channel);

    for (int i = 0; i < ts_count; i++) {
        if (raw_words[i] == 0u) {
            stat_inc(&stat_markers_100ms);
        }
    }

    if (g_analyzer) {
        analyze_stream_feed(
            g_analyzer,
            channel,
            raw_timestamps,
            fronts,
            raw_words,
            ts_count,
            &wctx->ms_offsets_analyze[channel]
        );
    }

    if (g_capture.record_raw) {
        if (!files[channel]) {
            open_file((Channel_t)channel);
        }
        if (files[channel]) {
            write_timestamp_lines(
                files[channel],
                (Channel_t)channel,
                raw_timestamps,
                fronts,
                raw_words,
                ts_count,
                &wctx->ms_offsets_raw[channel]
            );

            wctx->packets_since_flush[channel]++;
            if (wctx->packets_since_flush[channel] >= FLUSH_EVERY_PACKETS) {
                fflush(files[channel]);
                wctx->packets_since_flush[channel] = 0;
            }
        }
    }
}

int enqueue_packet(const unsigned char* packet, int len, Channel_t channel) {
    pthread_mutex_lock(&queue_mutex);

    while (queue_count >= QUEUE_SIZE) {
        pthread_cond_wait(&queue_not_full, &queue_mutex);
    }

    unsigned char* data_copy = malloc(len);
    if (!data_copy) {
        pthread_mutex_unlock(&queue_mutex);
        return -1;
    }

    memcpy(data_copy, packet, len);

    queue[queue_tail].data = data_copy;
    queue[queue_tail].len = len;
    queue[queue_tail].channel = channel;

    queue_tail = (queue_tail + 1) % QUEUE_SIZE;
    queue_count++;

    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&queue_mutex);

    return 0;
}

int dequeue_packet(packet_info_t* out) {
    pthread_mutex_lock(&queue_mutex);

    while (queue_count == 0 && !capture_finished) {
        pthread_cond_wait(&queue_not_empty, &queue_mutex);
    }

    if (queue_count == 0 && capture_finished) {
        pthread_mutex_unlock(&queue_mutex);
        return -1;
    }

    out->data = queue[queue_head].data;
    out->len = queue[queue_head].len;
    out->channel = queue[queue_head].channel;

    queue_head = (queue_head + 1) % QUEUE_SIZE;
    queue_count--;

    pthread_cond_signal(&queue_not_full);
    pthread_mutex_unlock(&queue_mutex);
    return 0;
}

void* write_thread(void* arg) {
    (void)arg;
    packet_info_t pkt;
    write_thread_ctx_t wctx;
    PacketReorder reorder;
    memset(&wctx, 0, sizeof(wctx));

    if (g_analyzer) {
        packet_reorder_configure_analyze(
            &reorder,
            g_capture.photon_channel,
            g_capture.trigger_channel
        );
        fprintf(stderr,
                "packet_collector: reorder ON photon=ch%d trigger=ch%d step=2\n",
                g_capture.photon_channel,
                g_capture.trigger_channel);
    } else {
        packet_reorder_init(&reorder);
    }

    while (1) {
        if (dequeue_packet(&pkt) < 0) {
            break;
        }

        if (pkt.channel < 0 || pkt.channel >= MAX_CHANNELS) {
            free(pkt.data);
            continue;
        }

        uint16_t counter = packet_payload_counter(pkt.data);
        packet_reorder_submit(
            &reorder,
            (int)pkt.channel,
            counter,
            pkt.data,
            pkt.len,
            process_ordered_packet,
            &wctx
        );
    }

    packet_reorder_flush(&reorder, process_ordered_packet, &wctx);
    stat_reorder_late_drops = packet_reorder_late_drops(&reorder);
    stat_reorder_pending_peak = packet_reorder_pending_peak(&reorder);
    stat_reorder_overflow = packet_reorder_overflow(&reorder);

    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (files[i]) {
            fflush(files[i]);
        }
    }

    return NULL;
}

void* capture_thread(void* arg) {
    void** args = (void**)arg;
    int sock = *(int*)args[0];
    void* ring = args[1];

    unsigned int idx = 0;
    int saved_pkt_num = -1;
    unsigned long frames_since_stats = 0;
    unsigned long frames_since_periodic = 0;

    while (1) {
        int processed = 0;

        while (1) {
            struct tpacket_hdr *hdr =
                (struct tpacket_hdr*)((char*)ring + idx * FRAME_SIZE);

            if (!(hdr->tp_status & TP_STATUS_USER)) {
                break;
            }

            processed = 1;

            unsigned char *packet = (unsigned char*)hdr + hdr->tp_mac;
            int len = hdr->tp_snaplen;

            if (len == 1066) {
                int pkt_num = (packet[0x12] << 8) + packet[0x13];
                Channel_t ch = packet_channel(packet);

                if (ch == UNDEFINED) {
                    __sync_synchronize();
                    hdr->tp_status = TP_STATUS_KERNEL;
                    idx++;
                    if (idx >= FRAME_NR) {
                        idx = 0;
                    }
                    continue;
                }

                if (saved_pkt_num >= 0) {
                    uint16_t diff = pkt_num - saved_pkt_num;
                    if (diff != 1 && !(saved_pkt_num == 65535 && pkt_num == 0)) {
                        fprintf(stderr,
                                "packet_collector: IP ID gap prev=0x%04x curr=0x%04x "
                                "missing=%u enqueued=%lu\n",
                                (unsigned)saved_pkt_num,
                                (unsigned)pkt_num,
                                (unsigned)(diff - 1),
                                stat_packets_enqueued);
                        fflush(stderr);
                    }
                }
                saved_pkt_num = pkt_num;

                if (enqueue_packet(packet, len, ch) == 0) {
                    stat_inc(&stat_packets_enqueued);
                } else {
                    note_enqueue_failure();
                }

                frames_since_stats++;
                if (frames_since_stats >= KERNEL_STATS_EVERY) {
                    report_kernel_drops(sock);
                    frames_since_stats = 0;
                }

                frames_since_periodic++;
                if (frames_since_periodic >= PERIODIC_STATS_EVERY) {
                    print_periodic_stats(sock, (unsigned long)queue_count);
                    frames_since_periodic = 0;
                }
            } else {
                stat_inc(&stat_wrong_len);
            }

            __sync_synchronize();
            hdr->tp_status = TP_STATUS_KERNEL;

            idx++;
            if (idx >= FRAME_NR) {
                idx = 0;
            }
        }

        if (!keep_running && !processed) {
            break;
        }

        if (!processed) {
            struct pollfd pfd = {sock, POLLIN, 0};
            poll(&pfd, 1, 100);
        }
    }

    return NULL;
}

int start_capture_config(const CaptureConfig* config) {
    if (!config || !config->iface) {
        fprintf(stderr, "Usage: packet_capture <iface> [--analyze-stream] [--record-raw] "
                        "[--output-dir DIR] [-o pulses_grouped.txt] [--group-size N]\n");
        return -1;
    }

    g_capture = *config;
    if (g_capture.group_size <= 0) {
        g_capture.group_size = 400;
    }
    if (g_capture.photon_channel < 0) {
        g_capture.photon_channel = 0;
    }
    if (g_capture.trigger_channel < 0) {
        g_capture.trigger_channel = 2;
    }

    if (g_capture.output_dir && g_capture.output_dir[0]) {
        if (snprintf(g_output_dir, sizeof(g_output_dir), "%s", g_capture.output_dir)
            >= (int)sizeof(g_output_dir)) {
            fprintf(stderr, "packet_collector: output dir path too long\n");
            return -1;
        }
        if (ensure_output_dir(g_output_dir) != 0) {
            return -1;
        }
    } else {
        g_output_dir[0] = '\0';
    }

    if (g_capture.analyze_stream) {
        const char* analyze_output = resolve_analyze_output_path(&g_capture);
        if (!analyze_output) {
            fprintf(stderr, "packet_collector: analyze output path too long\n");
            return -1;
        }

        AnalyzeStreamConfig stream_cfg = {
            .output_path = analyze_output,
            .group_size = g_capture.group_size,
            .photon_channel = g_capture.photon_channel,
            .trigger_channel = g_capture.trigger_channel,
        };
        g_analyzer = analyze_stream_create(&stream_cfg);
        if (!g_analyzer) {
            return -1;
        }
        fprintf(stderr,
                "packet_collector: analyze-stream ON output=%s group_size=%d "
                "photon=ch%d trigger=ch%d record_raw=%d\n",
                stream_cfg.output_path,
                stream_cfg.group_size,
                stream_cfg.photon_channel,
                stream_cfg.trigger_channel,
                g_capture.record_raw);
        fflush(stderr);
    } else if (g_output_dir[0]) {
        fprintf(stderr, "packet_collector: output_dir=%s\n", g_output_dir);
        fflush(stderr);
    }

    signal(SIGINT, int_handler);
    signal(SIGPIPE, SIG_IGN);

    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock == -1) {
        perror("socket");
        return -1;
    }

    struct tpacket_req req;
    memset(&req, 0, sizeof(req));

    req.tp_block_size = BLOCK_SIZE;
    req.tp_block_nr   = BLOCK_NR;
    req.tp_frame_size = FRAME_SIZE;
    req.tp_frame_nr   = FRAME_NR;

    if (setsockopt(sock, SOL_PACKET, PACKET_RX_RING, &req, sizeof(req)) < 0) {
        perror("PACKET_RX_RING");
        return -1;
    }

    unsigned int if_index = if_nametoindex(config->iface);
    if (!if_index) {
        perror("if_nametoindex");
        return -1;
    }

    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));

    sll.sll_family   = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex  = if_index;

    if (bind(sock, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
        perror("bind");
        return -1;
    }

    void *ring = mmap(NULL, req.tp_block_size * req.tp_block_nr,
                      PROT_READ | PROT_WRITE, MAP_SHARED, sock, 0);

    if (ring == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    pthread_t capture_tid, worker_tid;
    void* capture_args[] = {&sock, ring, &req};

    pthread_create(&capture_tid, NULL, capture_thread, capture_args);
    pthread_create(&worker_tid, NULL, write_thread, NULL);

    pthread_join(capture_tid, NULL);

    capture_finished = 1;

    pthread_cond_broadcast(&queue_not_empty);

    pthread_join(worker_tid, NULL);

    if (g_analyzer) {
        analyze_stream_finish(g_analyzer);
    }

    print_capture_summary(sock);

    if (g_analyzer) {
        analyze_stream_destroy(g_analyzer);
        g_analyzer = NULL;
    }

    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (files[i]) {
            fclose(files[i]);
            files[i] = NULL;
        }
    }

    munmap(ring, req.tp_block_size * req.tp_block_nr);
    close(sock);

    return 0;
}

int start_capture(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <iface>\n", argv[0]);
        return -1;
    }
    CaptureConfig config = {
        .iface = argv[1],
        .record_raw = 1,
        .analyze_stream = 0,
        .output_path = "pulses_grouped.txt",
        .group_size = 400,
        .photon_channel = 0,
        .trigger_channel = 2,
    };
    return start_capture_config(&config);
}
