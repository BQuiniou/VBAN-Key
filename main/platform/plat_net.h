// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef PLAT_NET_H
#define PLAT_NET_H

// ESP-IDF Wi-Fi + UDP shim: bring up station mode and send datagrams.
// TODO: udp_send(peer, buf, len) once the config/peer model exists.
void plat_net_init(void);

#endif // PLAT_NET_H
