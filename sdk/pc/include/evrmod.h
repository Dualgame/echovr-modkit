/* evrmod.h - the echovr-modloader broker API for Echo VR PC plugins.
 *
 * The loader (winmm.dll, this prototype) loads mods\*.dll on a worker thread. A plugin that wants broker
 * services exports
 *
 *     __declspec(dllexport) void evrmod_plugin_init(const EvrmodApi *api);
 *
 * and the loader calls it once, right after the plugin's DllMain returns, with a pointer to the live broker.
 * Plugins that do NOT export it are loaded unchanged and simply run their own DllMain; the broker never
 * touches them (a broker hook installed later chains onto a standalone plugin's hook, see below).
 *
 * ABI 2: the broker is LIVE. It owns four shared hook sites and installs each one ONCE,
 * lazily, on the first registration for it; a site nobody registers for stays stock. Each installed hook calls
 * the registered callbacks in registration order (= plugin load order), each under its own SEH fault guard, then
 * the original. A callback only observes: it cannot veto the original call. Sites, PC 6323983201049540
 * (found by disassembly):
 *   tick          CR15Game vftable slot RVA 0x16d1100 (stock 0x5194f0), pointer swap; `game` = this
 *   send_global   CncaGameSpace::SendComponentEventGlobal core 0x50fed0, 15-byte detour
 *   send_event    CncaGameSpace::SendComponentEvent (one gamespace) 0x50fb60, 15-byte detour
 *   metric_log    remote-log submit 0x113960 (the click-metric JSON), 16-byte detour
 * If a site already holds a standalone plugin's jmp [rip+0] into a loaded module (or the tick slot points into
 * one), the broker chains onto it as rad-links does.
 *
 * How a plugin knows the broker is loading it, from inside its DllMain (before evrmod_plugin_init runs): the
 * loader sets EVRMOD_BROKER_ENV to the ABI number while it loads plugins and clears it after. A plugin that sees
 * ABI >= 2 there defers its shared hooks to evrmod_plugin_init and registers; with no variable (a plain injector,
 * the ABI-1 loader) it self-hooks in DllMain exactly as before and its evrmod_plugin_init does nothing.
 */
#ifndef EVRMOD_H
#define EVRMOD_H

#include <stdint.h>
#include <stddef.h>

#define EVRMOD_ABI_VERSION 2u              /* 1 = registrations stored only; 2 = broker live */
#define EVRMOD_API_VERSION EVRMOD_ABI_VERSION
#define EVRMOD_BROKER_ENV  "EVRMOD_BROKER"  /* set to the ABI number while the loader runs the plugins' DllMain */

/* Build identity of the host module (echovr.exe). */
typedef struct EvrmodBuild {
    uint32_t time_date_stamp;   /* PE FileHeader.TimeDateStamp     */
    uint32_t size_of_image;     /* PE OptionalHeader.SizeOfImage   */
    void    *image_base;        /* GetModuleHandle(NULL)           */
    int      is_known;          /* 1 = matches a profile below     */
    const char *name;           /* e.g. "echovr.exe PC 6323983201049540 (2023-05-03)" or "unknown" */
} EvrmodBuild;

/* Shared-hook callback shapes. These mirror the stock engine signatures rad-links uses
 * (TickFn / SendCoreFn / RemoteLogFn; found by disassembly @ 6323983201049540); `user` is the pointer given at registration.
 * The original is called by the broker after the last callback: a callback never calls it. */
typedef void (*EvrmodTickFn)(void *game, void *user);
typedef void (*EvrmodSendEventFn)(void *gamespace, const uint64_t *event, uint32_t flags, void *user);
typedef void (*EvrmodMetricFn)(const char *json, size_t len, void *user);

typedef struct EvrmodApi {
    uint32_t abi_version;                 /* == EVRMOD_ABI_VERSION; a plugin must check this first   */

    const EvrmodBuild *(*build)(void);    /* host build identity (never NULL)                         */
    uint64_t (*csym64)(const char *s);    /* CSymbol64 name hash (RAD CRC64 "jones")                  */
    uint64_t (*csym64_type)(const char *bare_class, int gpu); /* csym64(class + "Win10"[+"GPU"])      */

    /* Fault-guarded copy: returns 1 and fills dst on success, 0 if the source range faulted. */
    int (*read)(void *dst, const void *src, size_t n);

    /* Register a broker callback (ABI >= 2: installs the site on first use, then fires the callback). Returns 0
     * on success; -1 table full / NULL fn, -2 the site is neither stock nor a known hook (not installed), -3 the
     * host build is unknown. ABI 1 only stored the callback and never fired it. */
    int (*register_tick)(EvrmodTickFn fn, void *user);
    int (*register_send_event)(EvrmodSendEventFn fn, void *user);    /* SendComponentEvent 0x50fb60 (targeted) */
    int (*register_metric_log)(EvrmodMetricFn fn, void *user);

    /* ---- ABI >= 2 (appended; check abi_version before use) ---- */
    int (*register_send_global)(EvrmodSendEventFn fn, void *user);   /* SendComponentEventGlobal core 0x50fed0 */
} EvrmodApi;

#ifdef _WINDOWS_
/* The broker ABI that is loading this plugin right now (from DllMain), or 0 = not loaded by a live broker. */
static __inline unsigned evrmod_broker_loading(void)
{
    char v[8];
    DWORD n = GetEnvironmentVariableA(EVRMOD_BROKER_ENV, v, sizeof v);
    unsigned abi = 0;
    if (!n || n >= sizeof v) return 0;
    for (DWORD i = 0; i < n && v[i] >= '0' && v[i] <= '9'; i++) abi = abi * 10 + (unsigned)(v[i] - '0');
    return abi;
}
#endif

typedef void (*EvrmodPluginInit)(const EvrmodApi *api);

#endif /* EVRMOD_H */
