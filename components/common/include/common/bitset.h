// SPDX-FileCopyrightText: 2026 Benoit Quiniou
// SPDX-License-Identifier: MIT

#ifndef VBANKEY_BITSET_H
#define VBANKEY_BITSET_H

#include <stdbool.h>
#include <stdint.h>

static inline bool bitset_get(uint32_t bits, unsigned index)
{
    return ((bits >> index) & 1U) != 0U;
}

static inline uint32_t bitset_set(uint32_t bits, unsigned index)
{
    return bits | (1U << index);
}

static inline uint32_t bitset_clear(uint32_t bits, unsigned index)
{
    return bits & ~(1U << index);
}

static inline uint32_t bitset_put(uint32_t bits, unsigned index, bool value)
{
    return value ? bitset_set(bits, index) : bitset_clear(bits, index);
}

#endif // VBANKEY_BITSET_H
