#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PACKET_REORDER_MAX_PENDING 2048u
#define PACKET_REORDER_MAX_GAP     256u

typedef struct PacketReorder PacketReorder;

typedef void (*PacketReorderDeliverFn)(
    void* ctx,
    int channel,
    const unsigned char* data,
    int len
);

void packet_reorder_init(PacketReorder* ro);
void packet_reorder_configure_analyze(PacketReorder* ro, int photon_channel, int trigger_channel);

/* Takes ownership of data. Frees data after deliver or on late duplicate drop. */
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

#ifdef __cplusplus
}
#endif
