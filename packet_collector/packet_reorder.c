#include "packet_reorder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int counter_is_future(uint16_t counter, uint16_t expected, uint16_t step) {
    uint16_t diff = (uint16_t)(counter - expected);

    if (diff == 0) {
        return 0;
    }
    if (diff > 0x8000u) {
        return 0;
    }
    if (step == 0 || (diff % step) != 0) {
        return 0;
    }
    return diff <= (uint16_t)(PACKET_REORDER_MAX_GAP * step);
}

static int find_pending_index(PacketReorderChannel* ch, uint16_t counter) {
    for (int i = 0; i < ch->pending_count; i++) {
        if (ch->pending[i].counter == counter) {
            return i;
        }
    }
    return -1;
}

static void remove_pending_at(PacketReorderChannel* ch, int index) {
    free(ch->pending[index].data);
    if (index < ch->pending_count - 1) {
        ch->pending[index] = ch->pending[ch->pending_count - 1];
    }
    ch->pending_count--;
}

static int store_pending(PacketReorderChannel* ch, uint16_t counter, unsigned char* data, int len) {
    if (ch->pending_count >= (int)PACKET_REORDER_MAX_PENDING) {
        return -1;
    }
    if (find_pending_index(ch, counter) >= 0) {
        return -1;
    }

    ch->pending[ch->pending_count].counter = counter;
    ch->pending[ch->pending_count].data = data;
    ch->pending[ch->pending_count].len = len;
    ch->pending_count++;

    if (ch->pending_count > ch->pending_peak) {
        ch->pending_peak = ch->pending_count;
    }
    return 0;
}

static void deliver_packet(
    PacketReorderChannel* ch,
    unsigned char* data,
    int len,
    PacketReorderDeliverFn deliver,
    void* ctx,
    int channel
) {
    deliver(ctx, channel, data, len);
    free(data);
    (void)ch;
}

static void try_drain_channel(
    PacketReorderChannel* ch,
    int channel,
    PacketReorderDeliverFn deliver,
    void* ctx
) {
    while (1) {
        int index = find_pending_index(ch, ch->next_expected);
        if (index < 0) {
            break;
        }

        PacketReorderPending slot = ch->pending[index];
        remove_pending_at(ch, index);
        deliver_packet(ch, slot.data, slot.len, deliver, ctx, channel);
        ch->next_expected = (uint16_t)(ch->next_expected + ch->counter_step);
    }
}

void packet_reorder_init(PacketReorder* ro) {
    memset(ro, 0, sizeof(*ro));
}

void packet_reorder_configure_analyze(PacketReorder* ro, int photon_channel, int trigger_channel) {
    packet_reorder_init(ro);

    if (photon_channel >= 0 && photon_channel < PACKET_REORDER_MAX_CHANNELS) {
        ro->channels[photon_channel].enabled = 1;
        ro->channels[photon_channel].counter_step = 2;
    }
    if (trigger_channel >= 0 && trigger_channel < PACKET_REORDER_MAX_CHANNELS) {
        ro->channels[trigger_channel].enabled = 1;
        ro->channels[trigger_channel].counter_step = 2;
    }
}

int packet_reorder_submit(
    PacketReorder* ro,
    int channel,
    uint16_t counter,
    unsigned char* data,
    int len,
    PacketReorderDeliverFn deliver,
    void* ctx
) {
    if (!ro || !data || len <= 0 || channel < 0 || channel >= PACKET_REORDER_MAX_CHANNELS) {
        free(data);
        return -1;
    }

    PacketReorderChannel* ch = &ro->channels[channel];
    if (!ch->enabled) {
        deliver_packet(ch, data, len, deliver, ctx, channel);
        return 0;
    }

    if (!ch->have_next) {
        ch->next_expected = counter;
        ch->have_next = 1;
    }

    if (counter == ch->next_expected) {
        deliver_packet(ch, data, len, deliver, ctx, channel);
        ch->next_expected = (uint16_t)(ch->next_expected + ch->counter_step);
        try_drain_channel(ch, channel, deliver, ctx);
        return 0;
    }

    if (counter_is_future(counter, ch->next_expected, ch->counter_step)) {
        if (store_pending(ch, counter, data, len) != 0) {
            ch->overflow++;
            if (ch->overflow <= 3) {
                fprintf(stderr,
                        "packet_reorder: pending full ch=%d counter=%u expected=%u\n",
                        channel,
                        (unsigned)counter,
                        (unsigned)ch->next_expected);
            }
            free(data);
            return -1;
        }
        return 0;
    }

    ch->late_drops++;
    if (ch->late_drops <= 3) {
        fprintf(stderr,
                "packet_reorder: late/duplicate ch=%d counter=%u expected=%u (dropped)\n",
                channel,
                (unsigned)counter,
                (unsigned)ch->next_expected);
    }
    free(data);
    return 0;
}

void packet_reorder_flush(PacketReorder* ro, PacketReorderDeliverFn deliver, void* ctx) {
    if (!ro) {
        return;
    }

    for (int channel = 0; channel < PACKET_REORDER_MAX_CHANNELS; channel++) {
        PacketReorderChannel* ch = &ro->channels[channel];
        if (!ch->enabled) {
            continue;
        }

        try_drain_channel(ch, channel, deliver, ctx);

        if (ch->pending_count > 0) {
            fprintf(stderr,
                    "packet_reorder: flush ch=%d pending=%d expected=%u (gaps remain)\n",
                    channel,
                    ch->pending_count,
                    (unsigned)ch->next_expected);
            for (int i = 0; i < ch->pending_count; i++) {
                free(ch->pending[i].data);
            }
            ch->pending_count = 0;
        }
    }
}

unsigned long packet_reorder_late_drops(const PacketReorder* ro) {
    unsigned long total = 0;
    if (!ro) {
        return 0;
    }
    for (int i = 0; i < PACKET_REORDER_MAX_CHANNELS; i++) {
        total += ro->channels[i].late_drops;
    }
    return total;
}

unsigned long packet_reorder_pending_peak(const PacketReorder* ro) {
    int peak = 0;
    if (!ro) {
        return 0;
    }
    for (int i = 0; i < PACKET_REORDER_MAX_CHANNELS; i++) {
        if (ro->channels[i].pending_peak > peak) {
            peak = ro->channels[i].pending_peak;
        }
    }
    return (unsigned long)peak;
}

unsigned long packet_reorder_overflow(const PacketReorder* ro) {
    unsigned long total = 0;
    if (!ro) {
        return 0;
    }
    for (int i = 0; i < PACKET_REORDER_MAX_CHANNELS; i++) {
        total += ro->channels[i].overflow;
    }
    return total;
}
