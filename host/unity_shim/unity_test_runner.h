// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

// Minimal standalone re-implementation of ESP-IDF's TEST_CASE registration, so
// component test sources written the ESP-IDF-idiomatic way (TEST_CASE, no
// main()) also compile and run against the vendored upstream Unity on the host.
// It is pulled in through unity_config.h (enabled with -DUNITY_INCLUDE_CONFIG_H);
// the matching main() and vbankey_test_register() live in unity_runner.c.
#ifndef VBANKEY_UNITY_TEST_RUNNER_H
#define VBANKEY_UNITY_TEST_RUNNER_H

#define VBANKEY_TC_CONCAT2(a, b) a##b
#define VBANKEY_TC_CONCAT(a, b) VBANKEY_TC_CONCAT2(a, b)
#define VBANKEY_TC_UID(prefix) VBANKEY_TC_CONCAT(prefix, __LINE__)

struct vbankey_test_case
{
    char const* name;
    void (*fn)(void);
    char const* file;
    int line;
    struct vbankey_test_case* next;
};

void vbankey_test_register(struct vbankey_test_case* test_case);

// desc_ (the "[tag]" string) is accepted for source compatibility with the
// ESP-IDF macro; the standalone runner simply runs every registered case.
#define TEST_CASE(name_, desc_) \
    static void VBANKEY_TC_UID(vbankey_tc_fn_)(void); \
    static void __attribute__((constructor)) VBANKEY_TC_UID(vbankey_tc_reg_)(void) \
    { \
        (void) (desc_); \
        static struct vbankey_test_case VBANKEY_TC_UID(vbankey_tc_) = { \
            name_, &VBANKEY_TC_UID(vbankey_tc_fn_), __FILE__, __LINE__, NULL \
        }; \
        vbankey_test_register(&VBANKEY_TC_UID(vbankey_tc_)); \
    } \
    static void VBANKEY_TC_UID(vbankey_tc_fn_)(void)

#endif // VBANKEY_UNITY_TEST_RUNNER_H
