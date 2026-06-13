#pragma once

/* Extensible channel layout (active: ch0 photon + ch2 trigger). */

#define MAX_CHANNELS 4

typedef enum {
    CHANNEL_ROLE_IGNORE = 0,
    CHANNEL_ROLE_PHOTON = 1,
    CHANNEL_ROLE_TRIGGER = 2,
} ChannelRole;

typedef struct {
    ChannelRole roles[MAX_CHANNELS];
    int photon_channel;
    int trigger_channel;
} ChannelLayout;

static inline void channel_layout_default(ChannelLayout* layout) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        layout->roles[i] = CHANNEL_ROLE_IGNORE;
    }
    layout->roles[0] = CHANNEL_ROLE_PHOTON;
    layout->roles[2] = CHANNEL_ROLE_TRIGGER;
    layout->photon_channel = 0;
    layout->trigger_channel = 2;
}
