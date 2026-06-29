#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Full cv_odmr run: ~7200 packets per channel; buffer one missing slot worth. */
#define PACKET_REORDER_MAX_PENDING  8192u
#define PACKET_REORDER_MAX_CHANNELS 4

typedef struct {
    unsigned char* data;
    int len;
    uint16_t counter;
} PacketReorderPending;

typedef struct {
    int enabled;
    int have_next;
    uint16_t next_expected;
    uint16_t counter_step;
    PacketReorderPending pending[PACKET_REORDER_MAX_PENDING];
    int pending_count;
    int pending_peak;
    unsigned long late_drops;
    unsigned long overflow;
    unsigned long gap_skips;
} PacketReorderChannel;

typedef struct PacketReorder {
    PacketReorderChannel channels[PACKET_REORDER_MAX_CHANNELS];
} PacketReorder;

typedef void (*PacketReorderDeliverFn)(
    void* ctx,
    int channel,
    const unsigned char* data,
    int len
);

void packet_reorder_init(PacketReorder* ro);
void packet_reorder_configure_analyze(PacketReorder* ro, int photon_channel, int trigger_channel);

/* Takes ownership of data. Frees data after deliver or on duplicate drop. */
int packet_reorder_submit(
    PacketReorder* ro,
    int channel,
    uint16_t counter,
    unsigned char* data,
    int len,
    PacketReorderDeliverFn deliver,
    void* ctx
);

void packet_reorder_flush(PacketReorder* ro, PacketReorderDeliverFn deliver, void* ctx);

unsigned long packet_reorder_late_drops(const PacketReorder* ro);
unsigned long packet_reorder_pending_peak(const PacketReorder* ro);
unsigned long packet_reorder_overflow(const PacketReorder* ro);
unsigned long packet_reorder_gap_skips(const PacketReorder* ro);

#ifdef __cplusplus
}
#endif
