// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef PLAT_NET_H
#define PLAT_NET_H

#include "config/config.h"
#include "vban_net/vban_net.h"

#include <stdbool.h>

/**
 * Connect to the configured Wi-Fi network and open the VBAN UDP socket.
 *
 * The SSID comes from config.toml. Its matching password is loaded from
 * encrypted NVS and is passed to the Wi-Fi driver using RAM-only storage.
 * This function waits until the station obtains an address or exhausts its
 * connection retries.
 */
bool plat_net_init(const struct config* cfg, struct vban_net* net);

#endif // PLAT_NET_H
