// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef VBAN_H
#define VBAN_H

/* VBAN wire-format header and packet builders. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/** Number of bytes in a VBAN wire header. */
#define VBAN_HEADER_SIZE ((size_t) 28U)
/** Largest payload carried by one VBAN packet. */
#define VBAN_MAX_PAYLOAD_SIZE ((size_t) 1436U)
/** Largest complete VBAN packet. */
#define VBAN_MAX_PACKET_SIZE (VBAN_HEADER_SIZE + VBAN_MAX_PAYLOAD_SIZE)
/** Number of bytes available for a VBAN stream name. */
#define VBAN_STREAM_NAME_SIZE ((size_t) 16U)
/** Number of bits in each VBAN wire byte. */
#define VBAN_BITS_PER_BYTE ((uint32_t) 8U)

/** Low-five-bit sample-rate/bit-rate index mask. */
#define VBAN_SR_INDEX_MASK ((uint8_t) 0x1FU)
/** Compatibility name for the low-five-bit bit-rate index mask. */
#define VBAN_BPS_MASK VBAN_SR_INDEX_MASK
/** Top-three-bit VBAN sub-protocol mask. */
#define VBAN_PROTOCOL_MASK ((uint8_t) 0xE0U)

/** VBAN sub-protocol values stored in format_SR. */
enum vban_protocol
{
    /** Audio sub-protocol. */
    VBAN_PROTOCOL_AUDIO = 0x00,
    /** Serial sub-protocol, including MIDI. */
    VBAN_PROTOCOL_SERIAL = 0x20,
    /** Text sub-protocol. */
    VBAN_PROTOCOL_TEXT = 0x40,
    /** Service sub-protocol. */
    VBAN_PROTOCOL_SERVICE = 0x60
};

/** Bit-rate indices used by the provided builders. */
enum vban_sr_index
{
    /**
     * No particular bit rate. VBAN-MIDI has no COM baud rate to advertise, so
     * the spec ("Serial Header: SR gives BPS", p.15) says to set the index to
     * ZERO.
     */
    VBAN_SR_INDEX_NONE = 0,
    /** 256000 bps, used by the TEXT builder. */
    VBAN_SR_INDEX_TEXT_256000 = 18
};

/** SERIAL format_bit data types used by this header. */
enum vban_serial_type
{
    /** MIDI serial payload. */
    VBAN_SERIAL_TYPE_MIDI = 0x10
};

/** TXT format_bit data types. */
enum vban_text_type
{
    /** ASCII text payload. */
    VBAN_TEXT_TYPE_ASCII = 0x00,
    /** UTF-8 text payload. */
    VBAN_TEXT_TYPE_UTF8 = 0x10,
    /** Wide-character text payload. */
    VBAN_TEXT_TYPE_WCHAR = 0x20
};

/*
 * SERIAL "COM Port configuration" bitmode bits carried in format_nbs
 * (spec p.16). Regular MIDI port mode = 1 start bit, 1 stop bit, no parity,
 * which is only the START-BIT bit set.
 */
/** SERIAL format_nbs bit 2: a start bit is present. */
#define VBAN_SERIAL_START_BIT ((uint8_t) 0x04U)
/** SERIAL format_nbs bit 7: another fragment follows (multipart data block). */
#define VBAN_SERIAL_MORE_FRAGMENTS ((uint8_t) 0x80U)
/** Default MIDI format_nbs value: regular MIDI port mode (1 start, 1 stop, no parity). */
#define VBAN_MIDI_NBS_DEFAULT VBAN_SERIAL_START_BIT
/** Default MIDI format_bit value. */
#define VBAN_MIDI_FORMAT_BIT ((uint8_t) VBAN_SERIAL_TYPE_MIDI)
/** Default TEXT format_bit value. */
#define VBAN_TEXT_FORMAT_BIT ((uint8_t) VBAN_TEXT_TYPE_UTF8)
/** Default MIDI format_SR value: SERIAL sub-protocol, no particular bit rate. */
#define VBAN_MIDI_FORMAT_SR ((uint8_t) ((uint8_t) VBAN_PROTOCOL_SERIAL | (uint8_t) VBAN_SR_INDEX_NONE))
/** Default TEXT format_SR value. */
#define VBAN_TEXT_FORMAT_SR ((uint8_t) ((uint8_t) VBAN_PROTOCOL_TEXT | (uint8_t) VBAN_SR_INDEX_TEXT_256000))

/** Return codes used by the fallible builders. */
enum vban_result
{
    /** Operation completed successfully. */
    VBAN_OK = 0,
    /** A required pointer was null. */
    VBAN_ERROR_INVALID_ARGUMENT = -1,
    /** The MIDI input was empty. */
    VBAN_ERROR_EMPTY_PAYLOAD = -2,
    /** The output buffer was too small. */
    VBAN_ERROR_BUFFER_TOO_SMALL = -3,
    /** A size calculation could not be represented by size_t. */
    VBAN_ERROR_SIZE_OVERFLOW = -4
};

#ifdef _MSC_VER
#    pragma pack(push, 1)
#    define VBAN_PACKED
#else
#    define VBAN_PACKED __attribute__((packed))
#endif

/** Exact 28-byte VBAN wire header; multi-byte fields use explicit accessors. */
struct VBAN_PACKED vban_header
{
    /** VBAN FOURCC bytes. */
    uint8_t fourcc[4];
    /** Low five bits are the rate index; high three bits are the protocol. */
    uint8_t format_SR;
    /** Protocol-specific sample/bit-mode field. */
    uint8_t format_nbs;
    /** Protocol-specific channel field. */
    uint8_t format_nbc;
    /** Protocol-specific data-type field. */
    uint8_t format_bit;
    /** Zero-padded stream name, not necessarily NUL-terminated. */
    uint8_t streamname[VBAN_STREAM_NAME_SIZE];
    /** Per-stream frame number; access through the little-endian helpers. */
    uint32_t nuFrame;
};

#ifdef _MSC_VER
#    pragma pack(pop)
#endif
#undef VBAN_PACKED

/* Compile-time check that the VBAN wire header is exactly 28 bytes. */
typedef char vban_header_size_static_assert[(sizeof(struct vban_header) == VBAN_HEADER_SIZE) ? 1 : -1];

/** The four literal VBAN FOURCC bytes. Byte-wise, so host byte order is irrelevant. */
#define VBAN_FOURCC "VBAN"

/** Write the literal VBAN FOURCC without relying on host byte order. */
static inline void vban_header_write_fourcc(struct vban_header* header)
{
    memcpy(header->fourcc, VBAN_FOURCC, sizeof header->fourcc);
}

/** Return true when a header contains the literal VBAN FOURCC. */
static inline bool vban_header_has_fourcc(const struct vban_header* header)
{
    if (header == NULL)
    {
        return false;
    }
    return memcmp(header->fourcc, VBAN_FOURCC, sizeof header->fourcc) == 0;
}

/** Store a frame number in little-endian wire order. */
static inline void vban_header_write_frame(struct vban_header* header, uint32_t frame)
{
    uint8_t* bytes = (uint8_t*) header;
    size_t const frame_offset = offsetof(struct vban_header, nuFrame);

    bytes[frame_offset] = (uint8_t) frame;
    bytes[frame_offset + 1U] = (uint8_t) (frame >> VBAN_BITS_PER_BYTE);
    bytes[frame_offset + 2U] = (uint8_t) (frame >> (2U * VBAN_BITS_PER_BYTE));
    bytes[frame_offset + 3U] = (uint8_t) (frame >> (3U * VBAN_BITS_PER_BYTE));
}

/** Read a frame number from little-endian wire order. */
static inline uint32_t vban_header_read_frame(const struct vban_header* header)
{
    uint8_t const* bytes = (uint8_t const*) header;
    size_t const frame_offset = offsetof(struct vban_header, nuFrame);

    return (uint32_t) bytes[frame_offset] | ((uint32_t) bytes[frame_offset + 1U] << VBAN_BITS_PER_BYTE) |
           ((uint32_t) bytes[frame_offset + 2U] << (2U * VBAN_BITS_PER_BYTE)) |
           ((uint32_t) bytes[frame_offset + 3U] << (3U * VBAN_BITS_PER_BYTE));
}

/** Copy, truncate, and zero-pad an explicit-length stream name. */
static inline int vban_header_set_stream_name(struct vban_header* header, char const* name, size_t name_length)
{
    size_t copy_length;

    if (header == NULL || (name == NULL && name_length != 0U))
    {
        return VBAN_ERROR_INVALID_ARGUMENT;
    }
    copy_length = name_length < VBAN_STREAM_NAME_SIZE ? name_length : VBAN_STREAM_NAME_SIZE;
    if (copy_length != 0U)
    {
        memcpy(header->streamname, name, copy_length);
    }
    memset(header->streamname + copy_length, 0, VBAN_STREAM_NAME_SIZE - copy_length);
    return VBAN_OK;
}

/** Grouped inputs for the private header builder. */
struct vban_private_header_parameters
{
    char const* stream_name;
    size_t stream_name_length;
    uint32_t frame;
    uint8_t format_sr;
    uint8_t format_nbs;
    uint8_t format_bit;
};

static inline int vban_private_build_header(struct vban_header* header, const struct vban_private_header_parameters* parameters)
{
    int result;

    if (header == NULL)
    {
        return VBAN_ERROR_INVALID_ARGUMENT;
    }
    result = vban_header_set_stream_name(header, parameters->stream_name, parameters->stream_name_length);
    if (result != VBAN_OK)
    {
        return result;
    }
    vban_header_write_fourcc(header);
    header->format_SR = parameters->format_sr;
    header->format_nbs = parameters->format_nbs;
    header->format_nbc = 0U;
    header->format_bit = parameters->format_bit;
    vban_header_write_frame(header, parameters->frame);
    return VBAN_OK;
}

/** Build a UTF-8 VBAN-TEXT header using the 256000 bps defaults. */
static inline int
vban_build_text_header(struct vban_header* header, char const* stream_name, size_t stream_name_length, uint32_t frame)
{
    const struct vban_private_header_parameters parameters = {stream_name, stream_name_length,  frame, VBAN_TEXT_FORMAT_SR,
                                                              0U,          VBAN_TEXT_FORMAT_BIT};

    return vban_private_build_header(header, &parameters);
}

/** Build a VBAN-MIDI header using the regular MIDI port-mode defaults (no advertised bit rate). */
static inline int vban_build_midi_header(
    struct vban_header* header, char const* stream_name, size_t stream_name_length, uint32_t frame, bool more_fragments
)
{
    struct vban_private_header_parameters parameters = {stream_name,         stream_name_length,    frame,
                                                        VBAN_MIDI_FORMAT_SR, VBAN_MIDI_NBS_DEFAULT, VBAN_MIDI_FORMAT_BIT};

    if (more_fragments)
    {
        parameters.format_nbs |= VBAN_SERIAL_MORE_FRAGMENTS;
    }

    return vban_private_build_header(header, &parameters);
}

/**
 * Assemble raw MIDI bytes into contiguous VBAN-MIDI packets.
 *
 * On success, packet_count and bytes_written describe output_buffer and
 * next_frame is advanced once per packet (uint32_t wrap is intentional). On
 * failure, all three outputs and output_buffer are left unchanged.
 */
static inline int vban_assemble_midi(
    uint8_t const* midi, size_t midi_length, char const* stream_name, size_t stream_name_length, uint32_t first_frame,
    uint8_t* output_buffer, size_t output_capacity, size_t* packet_count, size_t* bytes_written, uint32_t* next_frame
)
{
    size_t packets;
    size_t required;
    size_t source_offset = 0U;
    size_t output_offset = 0U;
    size_t packet_index;

    if (midi == NULL || output_buffer == NULL || packet_count == NULL || bytes_written == NULL || next_frame == NULL ||
        (stream_name == NULL && stream_name_length != 0U))
    {
        return VBAN_ERROR_INVALID_ARGUMENT;
    }
    if (midi_length == 0U)
    {
        return VBAN_ERROR_EMPTY_PAYLOAD;
    }
    packets = 1U + ((midi_length - 1U) / VBAN_MAX_PAYLOAD_SIZE);
    if (packets > (SIZE_MAX - midi_length) / VBAN_HEADER_SIZE)
    {
        return VBAN_ERROR_SIZE_OVERFLOW;
    }
    required = midi_length + (packets * VBAN_HEADER_SIZE);
    if (required > output_capacity)
    {
        return VBAN_ERROR_BUFFER_TOO_SMALL;
    }

    for (packet_index = 0U; packet_index < packets; ++packet_index)
    {
        size_t remaining = midi_length - source_offset;
        size_t payload_length = remaining < VBAN_MAX_PAYLOAD_SIZE ? remaining : VBAN_MAX_PAYLOAD_SIZE;
        struct vban_header* header = (struct vban_header*) (output_buffer + output_offset);
        bool more_fragments = packet_index + 1U < packets;

        (void) vban_build_midi_header(
            header, stream_name, stream_name_length, first_frame + (uint32_t) packet_index, more_fragments
        );
        memcpy(output_buffer + output_offset + VBAN_HEADER_SIZE, midi + source_offset, payload_length);
        source_offset += payload_length;
        output_offset += VBAN_HEADER_SIZE + payload_length;
    }

    *packet_count = packets;
    *bytes_written = required;
    *next_frame = first_frame + (uint32_t) packets;
    return VBAN_OK;
}

#endif /* VBAN_H */
