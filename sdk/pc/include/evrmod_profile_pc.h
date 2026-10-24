/* evrmod_profile_pc.h - the ONE PC build profile shared by the echovr-modloader broker (winmm.dll) and its plugins
 * (the broker and every plugin include this one copy of the build stamp and of CSymbol64); a plugin takes both from
 * here, and under the broker it should prefer the broker's api->build / api->csym64 (evrmod.h) over this copy:
 * this header is the standalone fallback and the broker's own source of truth.
 *
 * Build identity: echovr.exe, Echo VR PC final 6323983201049540 (2023-05-03). TimeDateStamp + SizeOfImage are the
 * PE header fields the loader and every plugin compare (read from the stock echovr.exe of
 * 6323983201049540). Per-site RVAs and prologue bytes stay with the code that owns the site (the
 * broker's k_sites, a plugin's own profile); only the identity lives here.
 *
 * CSymbol64: RAD's 64-bit name hash, CRC64 "jones" variant, poly 0x95AC9329AC4BC9B5, init -1, ASCII lowercased.
 * The algorithm is the standard RAD CSymbol64 (names prefixed evrmod_).
 *   FileSymbol = evrmod_csym64(asset_name)
 *   TypeSymbol = evrmod_csym64(BareClass + "Win10" | "Win10GPU")   (PC win10 layers)
 * All functions are static __inline so a translation unit that uses only the stamp gets no unused-function
 * warning (C4505) under /W4. */
#ifndef EVRMOD_PROFILE_PC_H
#define EVRMOD_PROFILE_PC_H

#include <stdint.h>

/* ---- build identity: PC 6323983201049540 ---- */
#define EVRMOD_PC_FINAL_TS    0x6452dff6u    /* PE FileHeader.TimeDateStamp     */
#define EVRMOD_PC_FINAL_SOI   0x2231000u     /* PE OptionalHeader.SizeOfImage   */
#define EVRMOD_PC_FINAL_NAME  "echovr.exe PC 6323983201049540 (2023-05-03)"

static __inline int evrmod_pc_is_final(uint32_t ts, uint32_t soi)
{ return ts == EVRMOD_PC_FINAL_TS && soi == EVRMOD_PC_FINAL_SOI; }

/* ---- CSymbol64 ---- */
static uint64_t g_evrmod_csym_seeds[256];
static int g_evrmod_csym_ready;

static __inline void evrmod_csym64_init(void)
{
    const uint64_t poly = 0x95AC9329AC4BC9B5ull;
    for (int i = 0; i < 256; i++) {
        uint64_t v = (i & 0x80) ? 0x2B5926535897936Aull : 0;
        if (i & 0x40) v ^= poly;
        for (int s = 0x20; s; s >>= 1) {
            v <<= 1;
            if (i & s) v ^= poly;
        }
        g_evrmod_csym_seeds[i] = v << 1;
    }
    g_evrmod_csym_ready = 1;
}

/* Hash `s` continuing from `base` (~0 for a fresh hash), so a suffix can be appended without concatenating. */
static __inline uint64_t evrmod_csym64_cont(uint64_t base, const char *s)
{
    if (!g_evrmod_csym_ready) evrmod_csym64_init();
    uint64_t r = base;
    for (; *s; s++) {
        unsigned c = (unsigned char)*s;
        if (c >= 'A' && c <= 'Z') c += 32;
        r = (r << 8) ^ g_evrmod_csym_seeds[r >> 56] ^ c;
    }
    return r;
}

static __inline uint64_t evrmod_csym64(const char *s) { return evrmod_csym64_cont(~0ull, s); }

#endif /* EVRMOD_PROFILE_PC_H */
