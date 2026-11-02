// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#include "config/config.h"
#include "unity.h"

#include <string.h>

TEST_CASE("load a complete config", "[config]")
{
    static char const source[] = "[wifi]\n"
                                 "ssid = \"test-network\"\n"
                                 "dhcp = false\n"
                                 "ip_address = \"192.168.1.40\"\n"
                                 "netmask = \"255.255.255.0\"\n"
                                 "gateway = \"192.168.1.1\"\n"
                                 "dns_server = \"192.168.1.1\"\n"
                                 "\n"
                                 "[global]\n"
                                 "version = \"2.0.0\"\n"
                                 "\n"
                                 "[[text_out]]\n"
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
    TEST_ASSERT_FALSE(cfg.wifi_dhcp);
    TEST_ASSERT_EQUAL_STRING("192.168.1.40", cfg.wifi_ip);
    TEST_ASSERT_EQUAL_STRING("255.255.255.0", cfg.wifi_netmask);
    TEST_ASSERT_EQUAL_STRING("192.168.1.1", cfg.wifi_gateway);
    TEST_ASSERT_EQUAL_STRING("192.168.1.1", cfg.wifi_dns);
    TEST_ASSERT_EQUAL_UINT(1U, cfg.n_text_out);
    TEST_ASSERT_NOT_NULL(cfg.text_out[0].name);
    TEST_ASSERT_EQUAL_STRING("Command1", cfg.text_out[0].name);
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

TEST_CASE("wifi defaults to DHCP", "[config]")
{
    static char const source[] = "[global]\nversion = \"2.0.0\"\n[wifi]\nssid = \"test-network\"\n";
    struct config cfg;
    char err[200];

    TEST_ASSERT_EQUAL(CONFIG_OK, config_load(source, (int) strlen(source), &cfg, err, sizeof err));
    TEST_ASSERT_TRUE(cfg.wifi_dhcp);
    TEST_ASSERT_NULL(cfg.wifi_ip);
    config_free(&cfg);
}

TEST_CASE("static wifi requires address netmask and gateway", "[config]")
{
    static char const source[] = "[global]\n"
                                 "version = \"2.0.0\"\n"
                                 "[wifi]\n"
                                 "ssid = \"test-network\"\n"
                                 "dhcp = false\n"
                                 "ip_address = \"192.168.1.40\"\n";
    struct config cfg;
    char err[200];

    TEST_ASSERT_EQUAL(CONFIG_ERR_SCHEMA, config_load(source, (int) strlen(source), &cfg, err, sizeof err));
}

static int load(char const* source, struct config* cfg, char* err, size_t err_len)
{
    return config_load(source, (int) strlen(source), cfg, err, err_len);
}

static void expect_rejected(char const* source, char const* message)
{
    struct config cfg;
    char err[200];

    TEST_ASSERT_EQUAL(CONFIG_ERR_SCHEMA, load(source, &cfg, err, sizeof err));
    TEST_ASSERT_NOT_NULL(strstr(err, message));
}

TEST_CASE("config version major must be supported", "[config]")
{
    struct config cfg;
    char err[200];

    expect_rejected("[global]\nversion = \"0.9.0\"\n", "major 0 is not supported");
    expect_rejected("[global]\nversion = \"3.0.0\"\n", "major 3 is not supported");
    expect_rejected("[global]\nversion = \"2.0\"\n", "MAJOR.MINOR.PATCH");
    expect_rejected("[global]\nversion = 2\n", "MAJOR.MINOR.PATCH");
    TEST_ASSERT_EQUAL(CONFIG_OK, load("[global]\nversion = \"2.4.1\"\n", &cfg, err, sizeof err));
    config_free(&cfg);
}

TEST_CASE("version 1 stream sections are converted", "[config]")
{
    static char const source[] = "[global]\nversion = \"1.0.0\"\n"
                                 "[[text]]\nid = 1\nstream_name = \"Command1\"\n"
                                 "[[midi]]\nid = 2\nstream_name = \"MIDI1\"\n";
    struct config cfg;
    char err[200];

    TEST_ASSERT_EQUAL(CONFIG_OK, load(source, &cfg, err, sizeof err));
    TEST_ASSERT_EQUAL_UINT(1U, cfg.n_text_out);
    TEST_ASSERT_EQUAL_STRING("Command1", cfg.text_out[0].name);
    TEST_ASSERT_EQUAL_UINT(1U, cfg.n_midi_out);
    TEST_ASSERT_EQUAL_INT(2, cfg.midi_out[0].id);
    config_free(&cfg);

    TEST_ASSERT_EQUAL(CONFIG_OK, load("[[text]]\nid = 1\n", &cfg, err, sizeof err));
    TEST_ASSERT_EQUAL_UINT(1U, cfg.n_text_out);
    config_free(&cfg);
}

TEST_CASE("sections and keys outside their config version are rejected", "[config]")
{
    expect_rejected(
        "[global]\nversion = \"2.0.0\"\n[[text]]\nid = 1\n", "[[text]] was renamed to [[text_out]] in config version 2"
    );
    expect_rejected(
        "[global]\nversion = \"2.0.0\"\n[[midi]]\nid = 1\n", "[[midi]] was renamed to [[midi_out]] in config version 2"
    );
    expect_rejected("[global]\nversion = \"1.0.0\"\n[[text_out]]\nid = 1\n", "[[text_out]] requires config version 2");
    expect_rejected(
        "[global]\nversion = \"1.0.0\"\n[[text_in]]\nstream_name = \"G\"\n", "[[text_in]] requires config version 2"
    );
    expect_rejected("[[gpo]]\nid = 1\ngpo_pin = 6\n", "[[gpo]] requires config version 2");
    expect_rejected("[[button]]\nid = 1\ngpi_pin = 4\ngpo = 1\n", "[[button]] gpo requires config version 2");
}

TEST_CASE("load text_in, gpo and a button driving a gpo", "[config]")
{
    static char const source[] = "[global]\nversion = \"2.0.0\"\n"
                                 "[[text_in]]\nstream_name = \"GPO1\"\nport = 6981\nip_address = \"192.168.1.57\"\n"
                                 "[[gpo]]\nid = 1\ngpo_pin = 6\n"
                                 "[[gpo]]\nid = 2\ngpo_pin = 7\nactive_low = true\n"
                                 "[[button]]\nid = 1\ngpi_pin = 4\ngpo = 2\n";
    struct config cfg;
    char err[200];

    TEST_ASSERT_EQUAL(CONFIG_OK, load(source, &cfg, err, sizeof err));
    TEST_ASSERT_EQUAL_UINT(1U, cfg.n_text_in);
    TEST_ASSERT_EQUAL_STRING("GPO1", cfg.text_in[0].name);
    TEST_ASSERT_EQUAL_UINT16(6981U, cfg.text_in[0].port);
    TEST_ASSERT_EQUAL_STRING("192.168.1.57", cfg.text_in[0].ip);
    TEST_ASSERT_EQUAL_UINT(2U, cfg.n_gpos);
    TEST_ASSERT_EQUAL_INT(6, cfg.gpos[0].gpo_pin);
    TEST_ASSERT_FALSE(cfg.gpos[0].active_low);
    TEST_ASSERT_TRUE(cfg.gpos[1].active_low);
    TEST_ASSERT_EQUAL_INT(2, cfg.buttons[0].gpo_id);
    TEST_ASSERT_EQUAL_PTR(&cfg.gpos[1], config_find_gpo(&cfg, 2));
    TEST_ASSERT_NULL(config_find_gpo(&cfg, 3));
    config_free(&cfg);
}

TEST_CASE("gpo and text_in references are validated", "[config]")
{
    expect_rejected("[global]\nversion = \"2.0.0\"\n[[text_in]]\nport = 6980\n", "requires a stream_name");
    expect_rejected("[global]\nversion = \"2.0.0\"\n[[gpo]]\nid = 0\ngpo_pin = 6\n", "positive and unique");
    expect_rejected(
        "[global]\nversion = \"2.0.0\"\n[[gpo]]\nid = 1\ngpo_pin = 6\n[[gpo]]\nid = 1\ngpo_pin = 7\n", "positive and unique"
    );
    expect_rejected(
        "[global]\nversion = \"2.0.0\"\n[[button]]\nid = 3\ngpi_pin = 4\ngpo = 9\n", "button 3 drives unknown gpo 9"
    );
    expect_rejected(
        "[global]\nversion = \"2.0.0\"\n[[gpo]]\nid = 1\ngpo_pin = 6\nactive_low = 1\n", "invalid configuration schema"
    );
}

TEST_CASE("a GPIO is assigned at most once", "[config]")
{
    expect_rejected(
        "[global]\nversion = \"2.0.0\"\n[[button]]\nid = 1\ngpi_pin = 4\n[[button]]\nid = 2\ngpi_pin = 4\n", "GPIO4"
    );
    expect_rejected("[global]\nversion = \"2.0.0\"\n[[gpo]]\nid = 1\ngpo_pin = 6\n[[gpo]]\nid = 2\ngpo_pin = 6\n", "GPIO6");
    expect_rejected(
        "[global]\nversion = \"2.0.0\"\n[[gpo]]\nid = 1\ngpo_pin = 5\n[[button]]\nid = 1\ngpi_pin = 5\ngpo = 1\n", "GPIO5"
    );
}

TEST_CASE("plaintext wifi passwords are rejected", "[config]")
{
    static char const source[] = "[global]\n"
                                 "version = \"2.0.0\"\n"
                                 "[wifi]\n"
                                 "ssid = \"test-network\"\n"
                                 "password = \"secret\"\n";
    struct config cfg;
    char err[200];

    TEST_ASSERT_EQUAL(CONFIG_ERR_SCHEMA, config_load(source, (int) strlen(source), &cfg, err, sizeof err));
}
