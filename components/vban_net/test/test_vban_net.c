// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "vban_net/vban_net.h"
#include "unity.h"

#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static ssize_t receive_packet(int sock, uint8_t* buffer, size_t capacity)
{
    ssize_t length = recvfrom(sock, buffer, capacity, 0, NULL, NULL);

    TEST_ASSERT_GREATER_OR_EQUAL_INT64(0, length);
    return length;
}

TEST_CASE("send text and MIDI over loopback", "[vban_net]")
{
    static char const text[] = "Strip(0).mute=1;";
    static uint8_t const midi[] = {0x90U, 0x3CU, 0x7FU};
    struct config cfg = {0};
    struct sockaddr_in address = {0};
    socklen_t address_length = sizeof address;
    struct timeval timeout = {1, 0};
    struct vban_net net;
    uint8_t buffer[VBAN_MAX_PACKET_SIZE];
    ssize_t length;
    int receiver;

    cfg.text_out[0] = (struct cfg_stream) {.id = 1, .ip = "127.0.0.1", .name = "Command1"};
    cfg.n_text_out = 1U;
    cfg.midi_out[0] = (struct cfg_stream) {.id = 1, .ip = "127.0.0.1", .name = "MIDI1"};
    cfg.n_midi_out = 1U;

    receiver = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, receiver);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0U);
    // NOLINTNEXTLINE(clang-analyzer-unix.StdCLibraryFunctions): receiver is guarded by the TEST_ASSERT above
    TEST_ASSERT_EQUAL_INT(0, bind(receiver, (struct sockaddr*) &address, sizeof address));
    TEST_ASSERT_EQUAL_INT(0, getsockname(receiver, (struct sockaddr*) &address, &address_length));
    TEST_ASSERT_EQUAL_INT(0, setsockopt(receiver, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout));
    cfg.text_out[0].port = ntohs(address.sin_port);
    cfg.midi_out[0].port = ntohs(address.sin_port);

    TEST_ASSERT_EQUAL_INT(0, vban_net_open(&net, &cfg));
    vban_net_send_text(&net, 0, text, 16U);
    length = receive_packet(receiver, buffer, sizeof buffer);
    TEST_ASSERT_EQUAL_INT64((ssize_t) (VBAN_HEADER_SIZE + 16U), length);
    TEST_ASSERT_EQUAL_MEMORY("VBAN", buffer, 4U);
    TEST_ASSERT_EQUAL_HEX8(0x40U, buffer[4] & 0xE0U);
    TEST_ASSERT_EQUAL_MEMORY("Command1", buffer + 8U, 8U);
    TEST_ASSERT_EQUAL_MEMORY(text, buffer + VBAN_HEADER_SIZE, 16U);
    TEST_ASSERT_EQUAL_UINT32(0U, vban_header_read_frame((const struct vban_header*) buffer));

    vban_net_send_text(&net, 0, text, 16U);
    (void) receive_packet(receiver, buffer, sizeof buffer);
    TEST_ASSERT_EQUAL_UINT32(1U, vban_header_read_frame((const struct vban_header*) buffer));

    vban_net_send_midi(&net, 0, midi, sizeof midi);
    length = receive_packet(receiver, buffer, sizeof buffer);
    TEST_ASSERT_EQUAL_INT64((ssize_t) (VBAN_HEADER_SIZE + sizeof midi), length);
    TEST_ASSERT_EQUAL_HEX8(0x20U, buffer[4] & 0xE0U);
    TEST_ASSERT_EQUAL_MEMORY(midi, buffer + VBAN_HEADER_SIZE, sizeof midi);

    vban_net_close(&net);
    (void) close(receiver);
}

static uint16_t free_udp_port(void)
{
    struct sockaddr_in address = {0};
    socklen_t address_length = sizeof address;
    int probe = socket(AF_INET, SOCK_DGRAM, 0);

    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, probe);
    if (probe < 0)
    {
        return 0U;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    TEST_ASSERT_EQUAL_INT(0, bind(probe, (struct sockaddr*) &address, sizeof address));
    TEST_ASSERT_EQUAL_INT(0, getsockname(probe, (struct sockaddr*) &address, &address_length));
    (void) close(probe);
    return ntohs(address.sin_port);
}

static void send_vban(int sock, uint16_t port, uint8_t format_sr, char const* name, char const* payload)
{
    uint8_t packet[VBAN_MAX_PACKET_SIZE];
    struct sockaddr_in target = {0};
    size_t const length = strlen(payload);

    TEST_ASSERT_EQUAL_INT(VBAN_OK, vban_build_text_header((struct vban_header*) packet, name, strlen(name), 0U));
    ((struct vban_header*) packet)->format_SR = format_sr;
    memcpy(packet + VBAN_HEADER_SIZE, payload, length + 1U);
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    target.sin_port = htons(port);
    TEST_ASSERT_EQUAL_INT64(
        (ssize_t) (VBAN_HEADER_SIZE + length),
        sendto(sock, packet, VBAN_HEADER_SIZE + length, 0, (struct sockaddr*) &target, sizeof target)
    );
}

TEST_CASE("receive only matching TEXT on text_in streams", "[vban_net]")
{
    struct config cfg = {0};
    struct vban_net net;
    char text[VBAN_MAX_PAYLOAD_SIZE + 1U];
    uint16_t const port = free_udp_port();
    int sender = socket(AF_INET, SOCK_DGRAM, 0);

    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, sender);
    if (sender < 0)
    {
        return;
    }
    cfg.text_in[0] = (struct cfg_stream) {.id = 1, .port = port, .name = "GPO1"};
    cfg.text_in[1] = (struct cfg_stream) {.id = 2, .ip = "10.9.9.9", .port = port, .name = "Remote"};
    cfg.n_text_in = 2U;
    TEST_ASSERT_EQUAL_INT(0, vban_net_open(&net, &cfg));
    TEST_ASSERT_EQUAL_INT(net.text_in[0].frame, net.text_in[1].frame);

    send_vban(sender, port, VBAN_TEXT_FORMAT_SR, "GPO1", "GPO[1] = 1;");
    TEST_ASSERT_EQUAL_INT(0, vban_net_receive_text(&net, 1000U, text, sizeof text));
    TEST_ASSERT_EQUAL_STRING("GPO[1] = 1;", text);

    send_vban(sender, port, VBAN_TEXT_FORMAT_SR, "GPO12", "GPO[1] = 0;");
    TEST_ASSERT_EQUAL_INT(-1, vban_net_receive_text(&net, 1000U, text, sizeof text));
    send_vban(sender, port, VBAN_MIDI_FORMAT_SR, "GPO1", "GPO[1] = 0;");
    TEST_ASSERT_EQUAL_INT(-1, vban_net_receive_text(&net, 1000U, text, sizeof text));
    send_vban(sender, port, VBAN_TEXT_FORMAT_SR, "Remote", "GPO[1] = 0;");
    TEST_ASSERT_EQUAL_INT(-1, vban_net_receive_text(&net, 1000U, text, sizeof text));
    TEST_ASSERT_EQUAL_INT(-1, vban_net_receive_text(&net, 10U, text, sizeof text));

    vban_net_close(&net);
    (void) close(sender);
}
