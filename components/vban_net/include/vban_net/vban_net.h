// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef VBANKEY_VBAN_NET_H
#define VBANKEY_VBAN_NET_H

#include <stddef.h>
#include <stdint.h>
#if defined(ESP_PLATFORM) && !defined(__linux__) && !defined(__APPLE__)
#    include <lwip/sockets.h> // real ESP chip firmware: struct sockaddr_in lives in LwIP
#else
#    include <netinet/in.h>   // native host, including the ESP-IDF "linux" target
#endif

#include "config/config.h"
#include "vban/vban.h"

struct vban_net
{
    int sock;
    const struct config* cfg;
    struct sockaddr_in text_addr[CFG_MAX_STREAMS];
    struct sockaddr_in midi_addr[CFG_MAX_STREAMS];
    uint32_t text_frame[CFG_MAX_STREAMS];
    uint32_t midi_frame[CFG_MAX_STREAMS];
};

int vban_net_open(struct vban_net* net, const struct config* cfg);
void vban_net_close(struct vban_net* net);
void vban_net_send_text(void* ctx, int stream_idx, char const* payload, size_t len);
void vban_net_send_midi(void* ctx, int stream_idx, uint8_t const* bytes, size_t len);

#endif // VBANKEY_VBAN_NET_H
