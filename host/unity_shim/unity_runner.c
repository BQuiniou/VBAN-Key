// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

// Standalone-host test runner: collects the TEST_CASE()s that the component test
// sources register via constructors, and runs them all under Unity — providing
// the main() that ESP-IDF's unit-test-app / host_test project supplies on the
// device and Linux targets. See unity_test_runner.h.
#include "unity.h"

#include "unity_test_runner.h"

#include <stddef.h>

// Weak no-op fixtures so a component's test/ dir can hold several TEST_CASE files
// without each one colliding on setUp/tearDown; a suite overrides them (strong
// definitions) only when it needs a fixture. ESP-IDF's Unity provides the same
// weak defaults (unity_compat.c) for the on-target and idf-linux builds.
__attribute__((weak)) void setUp(void) {}

__attribute__((weak)) void tearDown(void) {}

static struct vbankey_test_case* s_head = NULL;
static struct vbankey_test_case* s_tail = NULL;

void vbankey_test_register(struct vbankey_test_case* test_case)
{
    if (s_tail != NULL)
    {
        s_tail->next = test_case;
    }
    else
    {
        s_head = test_case;
    }
    s_tail = test_case;
}

int main(void)
{
    struct vbankey_test_case* test_case;

    UNITY_BEGIN();
    for (test_case = s_head; test_case != NULL; test_case = test_case->next)
    {
        // Report failures against the test source, not this runner.
        Unity.TestFile = test_case->file;
        UnityDefaultTestRun(test_case->fn, test_case->name, test_case->line);
    }
    return UNITY_END();
}
