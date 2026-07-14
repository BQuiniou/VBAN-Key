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

    cfg.text[0] = (struct cfg_stream) {.id = 1, .ip = "127.0.0.1", .name = "Command1"};
    cfg.n_text = 1U;
    cfg.midi[0] = (struct cfg_stream) {.id = 1, .ip = "127.0.0.1", .name = "MIDI1"};
    cfg.n_midi = 1U;

    receiver = socket(AF_INET, SOCK_DGRAM, 0);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, receiver);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0U);
    // NOLINTNEXTLINE(clang-analyzer-unix.StdCLibraryFunctions): receiver is guarded by the TEST_ASSERT above
    TEST_ASSERT_EQUAL_INT(0, bind(receiver, (struct sockaddr*) &address, sizeof address));
    TEST_ASSERT_EQUAL_INT(0, getsockname(receiver, (struct sockaddr*) &address, &address_length));
    TEST_ASSERT_EQUAL_INT(0, setsockopt(receiver, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout));
    cfg.text[0].port = ntohs(address.sin_port);
    cfg.midi[0].port = ntohs(address.sin_port);

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
