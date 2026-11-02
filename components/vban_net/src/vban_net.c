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
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

static uint16_t stream_port(const struct config* cfg, const struct cfg_stream* stream)
{
    return stream->port != 0U ? stream->port : cfg->default_port;
}

static void resolve(const struct config* cfg, const struct cfg_stream* stream, struct sockaddr_in* out)
{
    char const* ip;

    memset(out, 0, sizeof(*out));
    out->sin_family = AF_INET;
    out->sin_port = htons(stream_port(cfg, stream));
    ip = stream->ip != NULL && stream->ip[0] != '\0' ? stream->ip : cfg->default_ip;
    if (ip != NULL)
    {
        (void) inet_pton(AF_INET, ip, &out->sin_addr);
    }
}

static int shared_text_in_frame(const struct vban_net* net, size_t index)
{
    size_t other;

    for (other = 0U; other < index; ++other)
    {
        if (stream_port(net->cfg, &net->cfg->text_in[other]) == stream_port(net->cfg, &net->cfg->text_in[index]))
        {
            return net->text_in[other].frame;
        }
    }
    return -1;
}

static int open_text_in(struct vban_net* net, size_t index)
{
    const struct cfg_stream* stream = &net->cfg->text_in[index];
    struct vban_net_text_in* text_in = &net->text_in[index];
    struct sockaddr_in local = {0};
    int frame = shared_text_in_frame(net, index);

    text_in->from.s_addr = htonl(INADDR_ANY);
    if (stream->ip != NULL && stream->ip[0] != '\0' && inet_pton(AF_INET, stream->ip, &text_in->from) != 1)
    {
        return -1;
    }
    if (frame < 0)
    {
        frame = socket(AF_INET, SOCK_DGRAM, 0);
        if (frame < 0)
        {
            return -1;
        }
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = htons(stream_port(net->cfg, stream));
        if (bind(frame, (struct sockaddr*) &local, sizeof local) != 0)
        {
            (void) close(frame);
            return -1;
        }
    }
    text_in->frame = frame;
    return 0;
}

int vban_net_open(struct vban_net* net, const struct config* cfg)
{
    int on = 1;
    size_t index;

    net->cfg = cfg;
    for (index = 0U; index < CFG_MAX_STREAMS; ++index)
    {
        net->text_in[index].frame = -1;
    }
    net->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (net->sock < 0)
    {
        return -1;
    }
    (void) setsockopt(net->sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof on);
    for (index = 0U; index < cfg->n_text_out; ++index)
    {
        resolve(cfg, &cfg->text_out[index], &net->text_addr[index]);
        net->text_frame[index] = 0U;
    }
    for (index = 0U; index < cfg->n_midi_out; ++index)
    {
        resolve(cfg, &cfg->midi_out[index], &net->midi_addr[index]);
        net->midi_frame[index] = 0U;
    }
    for (index = 0U; index < cfg->n_text_in; ++index)
    {
        if (open_text_in(net, index) != 0)
        {
            vban_net_close(net);
            return -1;
        }
    }
    return 0;
}

void vban_net_close(struct vban_net* net)
{
    size_t index;

    if (net->sock >= 0)
    {
        (void) close(net->sock);
    }
    net->sock = -1;
    for (index = 0U; index < net->cfg->n_text_in; ++index)
    {
        if (net->text_in[index].frame >= 0 && shared_text_in_frame(net, index) < 0)
        {
            (void) close(net->text_in[index].frame);
        }
    }
    for (index = 0U; index < CFG_MAX_STREAMS; ++index)
    {
        net->text_in[index].frame = -1;
    }
}

static bool text_in_matches(
    const struct vban_net* net, size_t index, int frame, uint8_t const* packet, size_t length, const struct sockaddr_in* from
)
{
    const struct vban_header* header = (const struct vban_header*) packet;
    const struct vban_net_text_in* text_in = &net->text_in[index];
    char const* name = net->cfg->text_in[index].name;
    size_t const name_length = strlen(name);

    return text_in->frame == frame && length >= VBAN_HEADER_SIZE && vban_header_has_fourcc(header) &&
           (header->format_SR & VBAN_PROTOCOL_MASK) == VBAN_PROTOCOL_TEXT && name_length <= VBAN_STREAM_NAME_SIZE &&
           memcmp(header->streamname, name, name_length) == 0 &&
           (name_length == VBAN_STREAM_NAME_SIZE || header->streamname[name_length] == 0U) &&
           (text_in->from.s_addr == htonl(INADDR_ANY) || text_in->from.s_addr == from->sin_addr.s_addr);
}

int vban_net_receive_text(struct vban_net* net, uint32_t timeout_ms, char* text, size_t capacity)
{
    uint8_t packet[VBAN_MAX_PACKET_SIZE];
    struct timeval timeout = {(time_t) (timeout_ms / 1000U), (suseconds_t) ((timeout_ms % 1000U) * 1000U)};
    struct sockaddr_in from = {0};
    socklen_t from_length = sizeof from;
    fd_set readable;
    int max_frame = -1;
    int frame = -1;
    ssize_t length;
    size_t payload_length;
    size_t index;

    if (net == NULL || text == NULL || capacity == 0U)
    {
        return -1;
    }
    FD_ZERO(&readable);
    for (index = 0U; index < net->cfg->n_text_in; ++index)
    {
        FD_SET(net->text_in[index].frame, &readable);
        max_frame = net->text_in[index].frame > max_frame ? net->text_in[index].frame : max_frame;
    }
    if (max_frame < 0 || select(max_frame + 1, &readable, NULL, NULL, &timeout) <= 0)
    {
        return -1;
    }
    for (index = 0U; index < net->cfg->n_text_in && frame < 0; ++index)
    {
        if (FD_ISSET(net->text_in[index].frame, &readable))
        {
            frame = net->text_in[index].frame;
        }
    }
    length = recvfrom(frame, packet, sizeof packet, 0, (struct sockaddr*) &from, &from_length);
    if (length < 0)
    {
        return -1;
    }
    for (index = 0U; index < net->cfg->n_text_in; ++index)
    {
        if (text_in_matches(net, index, frame, packet, (size_t) length, &from))
        {
            payload_length = (size_t) length - VBAN_HEADER_SIZE;
            if (payload_length >= capacity)
            {
                return -1;
            }
            memcpy(text, packet + VBAN_HEADER_SIZE, payload_length);
            text[payload_length] = '\0';
            return (int) index;
        }
    }
    return -1;
}

void vban_net_send_text(void* ctx, int stream_idx, char const* payload, size_t len)
{
    struct vban_net* net = ctx;
    uint8_t buffer[VBAN_HEADER_SIZE + VBAN_MAX_PAYLOAD_SIZE];
    struct vban_header* header = (struct vban_header*) buffer;
    char const* name;
    size_t name_length;

    if (net == NULL || stream_idx < 0 || (size_t) stream_idx >= net->cfg->n_text_out)
    {
        return;
    }
    if (len > VBAN_MAX_PAYLOAD_SIZE)
    {
        return; /* TEXT has no continuation bit; refuse rather than send a truncated command. */
    }
    name = net->cfg->text_out[stream_idx].name;
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

    if (net == NULL || stream_idx < 0 || (size_t) stream_idx >= net->cfg->n_midi_out || len == 0U)
    {
        return;
    }
    name = net->cfg->midi_out[stream_idx].name;
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
