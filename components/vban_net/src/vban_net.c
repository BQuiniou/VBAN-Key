// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

// Expose POSIX/BSD socket symbols under strict ISO C (-std=c11); a no-op when GNU extensions are already enabled.
#define _DEFAULT_SOURCE 1 // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp): standard feature-test macro

#include "vban_net/vban_net.h"

#include "vban/vban.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void resolve(const struct config* cfg, const struct cfg_stream* stream, struct sockaddr_in* out)
{
    char const* ip;
    uint16_t port;

    memset(out, 0, sizeof(*out));
    out->sin_family = AF_INET;
    port = stream->port != 0U ? stream->port : cfg->default_port;
    out->sin_port = htons(port);
    ip = stream->ip != NULL && stream->ip[0] != '\0' ? stream->ip : cfg->default_ip;
    if (ip != NULL)
    {
        (void) inet_pton(AF_INET, ip, &out->sin_addr);
    }
}

int vban_net_open(struct vban_net* net, const struct config* cfg)
{
    int on = 1;
    size_t index;

    net->cfg = cfg;
    net->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (net->sock < 0)
    {
        return -1;
    }
    (void) setsockopt(net->sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof on);
    for (index = 0U; index < cfg->n_text; ++index)
    {
        resolve(cfg, &cfg->text[index], &net->text_addr[index]);
        net->text_frame[index] = 0U;
    }
    for (index = 0U; index < cfg->n_midi; ++index)
    {
        resolve(cfg, &cfg->midi[index], &net->midi_addr[index]);
        net->midi_frame[index] = 0U;
    }
    return 0;
}

void vban_net_close(struct vban_net* net)
{
    if (net->sock >= 0)
    {
        (void) close(net->sock);
    }
    net->sock = -1;
}

void vban_net_send_text(void* ctx, int stream_idx, char const* payload, size_t len)
{
    struct vban_net* net = ctx;
    uint8_t buffer[VBAN_HEADER_SIZE + VBAN_MAX_PAYLOAD_SIZE];
    struct vban_header* header = (struct vban_header*) buffer;
    char const* name;
    size_t name_length;

    if (net == NULL || stream_idx < 0 || (size_t) stream_idx >= net->cfg->n_text)
    {
        return;
    }
    if (len > VBAN_MAX_PAYLOAD_SIZE)
    {
        return; /* TEXT has no continuation bit; refuse rather than send a truncated command. */
    }
    name = net->cfg->text[stream_idx].name;
    name_length = name != NULL ? strlen(name) : 0U;
    (void) vban_build_text_header(header, name, name_length, net->text_frame[stream_idx]);
    memcpy(buffer + VBAN_HEADER_SIZE, payload, len);
    (void) sendto(
        net->sock, buffer, VBAN_HEADER_SIZE + len, 0, (struct sockaddr*) &net->text_addr[stream_idx],
        sizeof net->text_addr[stream_idx]
    );
    ++net->text_frame[stream_idx];
}

void vban_net_send_midi(void* ctx, int stream_idx, uint8_t const* bytes, size_t len)
{
    struct vban_net* net = ctx;
    char const* name;
    size_t name_length;
    size_t offset = 0U;

    if (net == NULL || stream_idx < 0 || (size_t) stream_idx >= net->cfg->n_midi || len == 0U)
    {
        return;
    }
    name = net->cfg->midi[stream_idx].name;
    name_length = name != NULL ? strlen(name) : 0U;
    while (offset < len)
    {
        uint8_t buffer[VBAN_HEADER_SIZE + VBAN_MAX_PAYLOAD_SIZE];
        struct vban_header* header = (struct vban_header*) buffer;
        size_t remaining = len - offset;
        size_t chunk = remaining > VBAN_MAX_PAYLOAD_SIZE ? VBAN_MAX_PAYLOAD_SIZE : remaining;
        bool more = offset + chunk < len;

        (void) vban_build_midi_header(header, name, name_length, net->midi_frame[stream_idx], more);
        memcpy(buffer + VBAN_HEADER_SIZE, bytes + offset, chunk);
        (void) sendto(
            net->sock, buffer, VBAN_HEADER_SIZE + chunk, 0, (struct sockaddr*) &net->midi_addr[stream_idx],
            sizeof net->midi_addr[stream_idx]
        );
        ++net->midi_frame[stream_idx];
        offset += chunk;
    }
}
