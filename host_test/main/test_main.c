// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

// Entry point for the ESP-IDF "linux" host-test build. With the FreeRTOS mock
// there is no scheduler/app_main, so we provide main() ourselves (as ESP-IDF's
// own host_tests do with Catch2WithMain) and drive ESP-IDF's Unity runner, which
// runs every TEST_CASE() the shared component test sources registered via
// constructors.
// Keep unity.h: UNITY_BEGIN/UNITY_END reach us via this umbrella header.
#include "unity.h" // IWYU pragma: keep

#include "unity_test_runner.h"

int main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    return UNITY_END();
}
