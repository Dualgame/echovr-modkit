/* CSymbol64 — RAD's 64-bit name hash (CRC64 "jones" variant, poly 0x95AC9329AC4BC9B5,
 * init -1, ASCII lowercased).
 *   FileSymbol = csym64(asset_name)
 *   TypeSymbol = csym64(BareClass + "Win10" | "Win10GPU")   (PC win10 layers) */
#pragma once
#include <stdint.h>

static uint64_t g_csym_seeds[256];
static int g_csym_ready;

static void csym64_init(void)
{
    const uint64_t poly = 0x95AC9329AC4BC9B5ull;
    for (int i = 0; i < 256; i++) {
        uint64_t v = (i & 0x80) ? 0x2B5926535897936Aull : 0;
        if (i & 0x40) v ^= poly;
        for (int s = 0x20; s; s >>= 1) {
            v <<= 1;
            if (i & s) v ^= poly;
        }
        g_csym_seeds[i] = v << 1;
    }
    g_csym_ready = 1;
}

/* Hash `s` continuing from `base` (-1 for a fresh hash), so a suffix can be appended
 * without building a concatenated string. */
static uint64_t csym64_cont(uint64_t base, const char *s)
{
    if (!g_csym_ready) csym64_init();
    uint64_t r = base;
    for (; *s; s++) {
        unsigned c = (unsigned char)*s;
        if (c >= 'A' && c <= 'Z') c += 32;
        r = (r << 8) ^ g_csym_seeds[r >> 56] ^ c;
    }
    return r;
}

static uint64_t csym64(const char *s) { return csym64_cont(~0ull, s); }
