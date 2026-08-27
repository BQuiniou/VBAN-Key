#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Benoit Quiniou
# SPDX-License-Identifier: MIT

"""Provision one Wi-Fi credential pair over the ESP32-C3 USB console."""

import argparse
import getpass
import string
import sys
import time

import serial


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="serial port, for example /dev/cu.usbmodem101")
    parser.add_argument("ssid", help="Wi-Fi SSID selected by config.toml")
    return parser.parse_args()


def read_password() -> str:
    password = getpass.getpass("Wi-Fi password (empty for an open network): ")
    confirmation = getpass.getpass("Confirm password: ")
    if password != confirmation:
        raise ValueError("passwords do not match")

    encoded = password.encode("utf-8")
    if encoded and not 8 <= len(encoded) <= 64:
        raise ValueError("password must contain 8..63 bytes, or 64 hexadecimal digits")
    if len(encoded) == 64 and any(character not in string.hexdigits for character in password):
        raise ValueError("a 64-byte password must contain only hexadecimal digits")
    return password


def make_command(ssid: str, password: str) -> bytes:
    encoded_ssid = ssid.encode("utf-8")
    encoded_password = password.encode("utf-8")

    if not 1 <= len(encoded_ssid) <= 32:
        raise ValueError("SSID must contain 1..32 bytes")

    password_field = encoded_password.hex() if encoded_password else "-"
    return f"SET {encoded_ssid.hex()} {password_field}\n".encode("ascii")


def provision(port: str, command: bytes) -> None:
    with serial.Serial(port, 115200, timeout=0.25, write_timeout=5) as connection:
        time.sleep(1)
        connection.reset_input_buffer()
        connection.write(command)
        connection.flush()

        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            response = connection.readline().decode("utf-8", errors="replace").strip()
            if response == "OK":
                return
            if response.startswith(("ERROR ", "FATAL ")):
                raise RuntimeError(response)

    raise TimeoutError("device did not acknowledge the credential")


def main() -> int:
    args = parse_args()
    try:
        password = read_password()
        provision(args.port, make_command(args.ssid, password))
    except (OSError, ValueError, RuntimeError, TimeoutError, serial.SerialException) as error:
        print(f"Provisioning failed: {error}", file=sys.stderr)
        return 1

    print("Wi-Fi credential stored.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
