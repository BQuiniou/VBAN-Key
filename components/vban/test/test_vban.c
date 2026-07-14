// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "vban/vban.h"
#include "unity.h"

#include <string.h>

TEST_CASE("build a text header", "[vban]")
{
    struct vban_header header;
    int result = vban_build_text_header(&header, "Command1", strlen("Command1"), 0U);

    TEST_ASSERT_EQUAL(VBAN_OK, result);
    // The 28-byte layout is guaranteed at compile time by vban.h
    // (vban_header_size_static_assert), so no runtime size check is needed here.
    TEST_ASSERT_TRUE(vban_header_has_fourcc(&header));
    TEST_ASSERT_EQUAL_UINT8(VBAN_TEXT_FORMAT_SR, header.format_SR);
    TEST_ASSERT_EQUAL_MEMORY("Command1", header.streamname, 8U);
    TEST_ASSERT_EQUAL_UINT32(0U, vban_header_read_frame(&header));
}
