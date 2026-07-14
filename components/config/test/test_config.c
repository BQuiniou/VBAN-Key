// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "config/config.h"
#include "unity.h"

#include <string.h>

TEST_CASE("load a complete config", "[config]")
{
    static char const source[] = "[wifi]\n"
                                 "ssid = \"test-network\"\n"
                                 "password = \"test-password\"\n"
                                 "\n"
                                 "[[text]]\n"
                                 "id = 1\n"
                                 "ip_address = \"192.168.1.57\"\n"
                                 "stream_name = \"Command1\"\n"
                                 "\n"
                                 "[[button]]\n"
                                 "id = 1\n"
                                 "type = \"latch\"\n"
                                 "gpi_pin = 4\n"
                                 "on = \"\"\"SendText(\"vban1\", on);\nWait(20);\"\"\"\n"
                                 "off = '''SendText(\"vban1\", off);'''\n";
    struct config cfg;
    char err[200];
    int result;

    result = config_load(source, (int) strlen(source), &cfg, err, sizeof err);
    TEST_ASSERT_EQUAL(CONFIG_OK, result);
    TEST_ASSERT_NOT_NULL(cfg.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING("test-network", cfg.wifi_ssid);
    TEST_ASSERT_EQUAL_UINT(1U, cfg.n_text);
    TEST_ASSERT_NOT_NULL(cfg.text[0].name);
    TEST_ASSERT_EQUAL_STRING("Command1", cfg.text[0].name);
    TEST_ASSERT_EQUAL_UINT(1U, cfg.n_buttons);
    TEST_ASSERT_EQUAL(BUTTON_LATCH, cfg.buttons[0].type);
    TEST_ASSERT_EQUAL_INT(4, cfg.buttons[0].gpi_pin);
    TEST_ASSERT_NULL(cfg.buttons[0].init);
    TEST_ASSERT_NOT_NULL(cfg.buttons[0].on);
    TEST_ASSERT_NOT_NULL(cfg.buttons[0].off);
    TEST_ASSERT_NOT_NULL(strstr(cfg.buttons[0].on, "SendText"));
    TEST_ASSERT_NOT_NULL(strstr(cfg.buttons[0].on, "Wait(20)"));
    TEST_ASSERT_NOT_NULL(strstr(cfg.buttons[0].off, "SendText"));

    config_free(&cfg);
}
