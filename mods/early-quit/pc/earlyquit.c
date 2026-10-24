/* evrmod_earlyquit.dll - the PC early-quit client half, restored in memory.
 *
 * RESEARCH / EDUCATIONAL. Echo VR PC final 6323983201049540 only (any other image is refused). The same four
 * sites, 23 bytes, as pc_exe_earlyquit_patch.py (the sites were found by disassembly), written into the running image
 * instead of a patched copy of echovr.exe, so the exe on disk stays stock:
 *   a  0x1401618b7  and dl,0xdd                -> and dl,0xff        FeatureFlags keeps every bit
 *   b1 0x1401ae2f0  cmp rax,-1; je +9          -> mov [rsp],r8; nop  keep the Update's 2nd long (setter home slot)
 *   b2 0x1401ae372  mov qword [rcx+0x64828],-1 -> push [rsp]; pop [rcx+0x64828]; nop   expiry = Update +0x20
 *   c  0x140d96319  mov byte [rbx],0           -> setg byte [rbx]    active = expiry != -1 && expiry > now
 * All four are checked before any is written: every site stock -> patch all; every site already patched (a
 * file-patched test exe) -> nothing to do; anything else -> refuse and write nothing. The optional --session-api
 * site `s` of the script is not applied here.
 *
 * Timing: under the evrmod broker (EVRMOD_BROKER >= 2 seen in DllMain) the patch runs in
 * evrmod_plugin_init, which the loader calls on the same worker thread right after this DllMain returns, and the
 * build check and the site reads go through the broker (api->build, api->read: fault-guarded). Standalone
 * (a plain injector's remote thread, or the ABI-1 loader, which sets no EVRMOD_BROKER) it runs in DllMain
 * as before, checking the stamp from evrmod_profile_pc.h and reading the sites under its own fault guard. Both
 * points are within milliseconds of each other and long before any site can run. None of the four sites can run
 * that early: a is the SNSEarlyQuitFeatureFlags
 * handler and b1/b2 the penalty setter behind SNSEarlyQuitUpdateNotification / the profile loader (both need the
 * login socket, seconds after start), and c is a script expression node that runs only once a level's scripts
 * evaluate (found by disassembly). So the in-memory patch equals the file patch for every
 * observable effect. A dedicated server never registers the handlers; the plugin is harmless
 * there but has no effect.
 *
 * Log: <exe dir>\_local\earlyquit.log (one line per site). Uninstall: delete the DLL; nothing is written to disk
 * except the log. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include "evrmod.h"               /* build_mods.cmd adds the include dir */
#include "evrmod_profile_pc.h"   /* the PC build stamp, shared with the broker */

static const struct { const char *name; uint32_t rva; uint8_t len; uint8_t stock[11], patched[11]; } k_sites[] = {
    { "a",  0x1618b7, 3,  { 0x80, 0xe2, 0xdd }, { 0x80, 0xe2, 0xff } },
    { "b1", 0x1ae2f0, 6,  { 0x48, 0x83, 0xf8, 0xff, 0x74, 0x09 }, { 0x4c, 0x89, 0x04, 0x24, 0x66, 0x90 } },
    { "b2", 0x1ae372, 11, { 0x48, 0xc7, 0x81, 0x28, 0x48, 0x06, 0x00, 0xff, 0xff, 0xff, 0xff },
                          { 0xff, 0x34, 0x24, 0x8f, 0x81, 0x28, 0x48, 0x06, 0x00, 0x66, 0x90 } },
    { "c",  0xd96319, 3,  { 0xc6, 0x03, 0x00 }, { 0x0f, 0x9f, 0x03 } },
};
#define NSITES (sizeof k_sites / sizeof k_sites[0])

static HANDLE g_log = INVALID_HANDLE_VALUE;
static int g_state;   /* 0 = not run / refused, 1 = patched now, 2 = was already patched */
static int g_brokered; /* DllMain saw a live broker (ABI >= 2): the patch waits for evrmod_plugin_init */

static void qlog(const char *fmt, ...)
{
    if (g_log == INVALID_HANDLE_VALUE) return;
    char line[512];
    SYSTEMTIME t; GetLocalTime(&t);
    int n = _snprintf_s(line, sizeof line, _TRUNCATE, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond,
                        t.wMilliseconds);
    va_list ap; va_start(ap, fmt);
    n += _vsnprintf_s(line + n, sizeof line - n, _TRUNCATE, fmt, ap);
    va_end(ap);
    if (n < 0 || n > (int)sizeof line - 2) n = (int)sizeof line - 2;
    line[n++] = '\n';
    DWORD wrote; WriteFile(g_log, line, n, &wrote, NULL);
}

static void open_log(void)
{
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, sizeof path);
    char *slash = n ? strrchr(path, '\\') : NULL;
    if (!slash) return;
    strcpy_s(slash + 1, sizeof path - (slash + 1 - path), "_local");
    CreateDirectoryA(path, NULL);
    strcat_s(path, sizeof path, "\\earlyquit.log");
    g_log = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);
}

static void hex(char *out, size_t cap, const uint8_t *p, size_t n)
{
    size_t k = 0;
    for (size_t i = 0; i < n && k + 3 < cap; i++) k += _snprintf_s(out + k, cap - k, _TRUNCATE, "%02x", p[i]);
}

typedef int (*ReadFn)(void *dst, const void *src, size_t n);

/* Standalone fault-guarded copy, the same contract as the broker's api->read. */
static int self_read(void *dst, const void *src, size_t n)
{
    __try { memcpy(dst, src, n); return 1; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

/* Patch the four sites of the image at `base`, whose identity (ts, soi) the caller has read. `rd` = the broker's
 * api->read, or self_read standalone. */
static void apply(uint8_t *base, uint32_t ts, uint32_t soi, ReadFn rd, const char *via)
{
    qlog("evrmod_earlyquit attach (%s), pid %lu, image base %p", via, GetCurrentProcessId(), base);
    if (!evrmod_pc_is_final(ts, soi)) {
        qlog("unsupported image: TimeDateStamp %08lx SizeOfImage %08lx: refusing", ts, soi);
        return;
    }
    qlog("profile: %s", EVRMOD_PC_FINAL_NAME);
    int nstock = 0, npatched = 0;
    for (size_t i = 0; i < NSITES; i++) {
        const uint8_t *at = base + k_sites[i].rva;
        uint8_t now_b[11];
        char cur[40];
        if (!rd(now_b, at, k_sites[i].len)) { qlog("site %s at %p: unreadable", k_sites[i].name, at); continue; }
        hex(cur, sizeof cur, now_b, k_sites[i].len);
        if (!memcmp(now_b, k_sites[i].stock, k_sites[i].len)) nstock++;
        else if (!memcmp(now_b, k_sites[i].patched, k_sites[i].len)) npatched++;
        else qlog("site %s at %p holds %s: neither stock nor patched", k_sites[i].name, at, cur);
    }
    if (npatched == (int)NSITES) { g_state = 2; qlog("all %u sites already patched (file-patched exe): nothing to do", (unsigned)NSITES); return; }
    if (nstock != (int)NSITES) {
        qlog("refusing: %d stock, %d patched of %u sites; nothing written", nstock, npatched, (unsigned)NSITES);
        return;
    }
    for (size_t i = 0; i < NSITES; i++) {
        uint8_t *at = base + k_sites[i].rva;
        DWORD old;
        char was[40], now[40];
        hex(was, sizeof was, at, k_sites[i].len);
        if (!VirtualProtect(at, k_sites[i].len, PAGE_EXECUTE_READWRITE, &old)) {
            qlog("site %s at %p: VirtualProtect failed (%lu); sites before it stay patched", k_sites[i].name, at,
                 GetLastError());
            return;
        }
        memcpy(at, k_sites[i].patched, k_sites[i].len);
        VirtualProtect(at, k_sites[i].len, old, &old);
        FlushInstructionCache(GetCurrentProcess(), at, k_sites[i].len);
        hex(now, sizeof now, at, k_sites[i].len);
        qlog("site %s at %p (VA %llx): %s -> %s", k_sites[i].name, at, 0x140000000ull + k_sites[i].rva, was, now);
    }
    g_state = 1;
    qlog("early-quit client half restored in memory (%u sites, 23 bytes)", (unsigned)NSITES);
}

/* Standalone: our own image, read from its PE header. */
static void apply_standalone(void)
{
    uint8_t *base = (uint8_t *)GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    apply(base, nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage, self_read, "standalone");
}

/* Broker entry. Under a live broker: the identity comes from api->build and the site reads go through
 * api->read; the writes stay here (the broker has no write service). Otherwise (DllMain already patched, or the
 * ABI-1 loader): only record the patch state. */
__declspec(dllexport) void evrmod_plugin_init(const EvrmodApi *api)
{
    if (!g_brokered) {
        qlog("evrmod broker ABI %u; patch state %s", api ? api->abi_version : 0,
             g_state == 1 ? "applied" : g_state == 2 ? "already patched" : "NOT applied");
        return;
    }
    if (!api || api->abi_version < 2) { qlog("broker ABI < 2 under EVRMOD_BROKER: early-quit off"); return; }
    const EvrmodBuild *b = api->build();
    if (!b || !b->is_known) { qlog("broker reports an unknown build (%s): refusing", b ? b->name : "NULL"); return; }
    apply((uint8_t *)b->image_base, b->time_date_stamp, b->size_of_image, api->read, "evrmod broker");
    qlog("evrmod broker ABI %u (%s); patch state %s", api->abi_version, b->name,
         g_state == 1 ? "applied" : g_state == 2 ? "already patched" : "NOT applied");
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        open_log();
        g_brokered = evrmod_broker_loading() >= 2;
        if (g_brokered) qlog("evrmod_earlyquit: broker ABI %u loading: patch deferred to evrmod_plugin_init",
                             evrmod_broker_loading());
        else apply_standalone();
    }
    return TRUE;
}
