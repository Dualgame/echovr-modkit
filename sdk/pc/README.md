# echovr-modloader — a winmm.dll proxy + mod loader for Echo VR PC

Research / educational. A clean-room, side-by-side DLL that Echo VR PC loads during start-up and that then
loads your own `mods\*.dll`. No injector is needed: Windows loads it like any DLL beside the executable. LAN use.

Target build: **Echo VR PC `6323983201049540`** (2023-05-03). It refuses to load plugins on any other image.

## Why winmm.dll

`echovr.exe` statically imports `winmm.dll` (4 functions) and so does `OculusSpatializerWwise.dll` (3), so
Windows loads a `winmm.dll` sitting next to `echovr.exe` during import resolution, before the game's entry
point (verified on PC `6323983201049540`).
`winmm` is **not** a KnownDLL on this host (checked: the key has no `winmm`/`hid` entry), so the app-directory
copy wins the search. It is not `dbgcore.dll`, which `EchoRelay.Patch` uses. Fallback target: `hid.dll`.

## How it forwards (the pitfall)

The proxy is itself named `winmm.dll`, so a forwarder to the module `winmm` would name the proxy again and
loop. A PE forwarder that names the system copy by full path is rejected by the MSVC linker (it reads the
path as an internal symbol). So:

- The **7 functions anything in this process imports** — `timeBeginPeriod`, `timeEndPeriod`,
  `timeGetDevCaps`, `timeGetTime` (echovr) plus `timeGetSystemTime`, `timeKillEvent`, `timeSetEvent`
  (Oculus) — are **real wrappers**. On first *call* (after `DllMain`, off the loader lock) each loads the
  system `winmm.dll` by its absolute `GetSystemDirectory` path and tail-calls through. **Echo VR therefore
  needs no extra file.**
- The **other 174 exports** are forwarders to a renamed copy `winmm_orig`, present only so the proxy's
  export table is byte-for-byte the system one (181 exports, same ordinals, same names). Nothing in this
  process imports them, so they are never resolved at runtime. `tools/gen_def.py` also emits
  `src/winmm_orig.def`; `build.cmd` turns it into an import library so the forwarders link. If some *other*
  program ever needs one of those 174, drop a copy of `C:\Windows\System32\winmm.dll` next to the exe,
  renamed `winmm_orig.dll` — Echo VR does not.

## Build

```bat
build.cmd            :: MSVC x64 (VS 2022 Build Tools), static CRT. Regenerates the .defs (needs python+pefile),
                     :: builds winmm_orig.lib, links build\winmm.dll.
build.cmd --no-def   :: skip the .def regeneration (use the committed src\winmm.def / winmm_orig.def)
python tools\gen_def.py check build\winmm.dll   :: assert the export table matches the system winmm
```

## Install

1. Copy `build\winmm.dll` next to `echovr.exe` (`<game>\bin\win10\`).
2. Make a `mods\` folder beside the exe and drop plugins in it, e.g. `mods\rad-links.dll` and
   `mods\evrmod_earlyquit.dll` (see `build_mods.cmd`).
3. Launch the game normally. No injector, no suspended start.

### Load order

The loader runs on a worker thread (never in `DllMain`) and loads:

1. the names in `mods\load_order.txt` if that file exists (one DLL name per line, `#` comments, blank lines
   ignored, case-insensitive; a listed name that is not in `mods\` or not a `.dll` is logged and skipped), then
2. every other `mods\*.dll` alphabetically. No name is special (the loader works with any
   subset of plugins present). The ordering is a pure function, `plan_load_order`, unit-tested
   by `build\broker_selftest.exe`.

It writes `mods\evrmod.log` (build identity, the final numbered load order, each plugin loaded/failed). It refuses to load any plugin if the
host `echovr.exe` is not build `6323983201049540` (PE timestamp + image size check).

## The broker (`include/evrmod.h`)

A plugin that wants shared services exports `void evrmod_plugin_init(const EvrmodApi *api)`; the loader calls
it right after the plugin's `DllMain`. The broker offers build identity, `CSymbol64`, a fault-guarded read, and
registration for the four shared hook sites: per-frame tick (`CR15Game` vftable slot), `SendComponentEventGlobal`
core, `SendComponentEvent` (targeted) and the remote-log `MetricLog` submit. **ABI 2: live.**
The broker installs each site once, on the first registration for it (a site nobody registers for stays stock),
and calls the callbacks in registration order (= load order), each under its own fault guard, then the original.
If a standalone plugin already hooked a site, the broker chains onto it.

While it loads plugins the loader sets `EVRMOD_BROKER=<ABI>`; a plugin's `DllMain` that sees ABI >= 2 defers its
shared hooks to `evrmod_plugin_init`. Without it (a plain injector, the ABI-1 loader) the same DLL self-hooks as
before. `rad-links.dll` built from today's source does both; so its load order against other hooking plugins does
not matter under the broker.

`build_mods.cmd` builds `build\mods\{rad-links,evrmod_earlyquit}.dll` (no debug info unless `DEBUG=1`)
and runs `build\broker_selftest.exe`, an offline check of dispatch order, the fault guard, install-once and chaining
against a fake image, plus the load-order cases. The early-quit plugin `evrmod_earlyquit.dll` applies the four
`pc_exe_earlyquit_patch.py` sites in memory (all four checked first; log `_local\earlyquit.log`); the
exe stays stock. Under the broker it patches from `evrmod_plugin_init` with `api->build` / `api->read`; standalone,
from its `DllMain`.

`include/evrmod_profile_pc.h` is the one PC build profile: the `6323983201049540` stamp and a CSymbol64
copy. The loader, rad-links and earlyquit use it; under the broker the plugins prefer `api->build` / `api->csym64` /
`api->read` and keep the header as the standalone fallback.

## Quest loader (`libevrmod.so`)

Target build: **Echo VR Quest `6528386917200980`** (vc 4987566). A loader only: no broker, no exports, no hooks
(a Quest broker is ABI 3).
- **How it loads.** `quest_apk_build_release.py` step 6 appends `libevrmod.so` to `libr15.so`'s
  `DT_NEEDED`. Three edits make it work: the spare `.dynamic` slot 34 becomes the NEEDED entry, the name sits in
  the shared cave at `0x2a760f0`, and both string bounds are widened to reach it (`DT_STRSZ`, and the `.dynstr`
  section header's `sh_size`, which bionic's `ElfReader` checks when it reads `DT_NEEDED` from the file). The APK
  stays non-debuggable and has no `wrap.sh`.
- **What it does.** Its constructor runs after `libr15.so` is relocated and before `libr15.so`'s own constructors.
  It `dlopen`s every `libevrmod_*.so` in its own directory, in `strcmp` order, and logs each result to logcat with
  the tag `evrmod`. The builder installs each `--plugin X.so` as `libevrmod_<stem>.so` (`libradlinks.so` →
  `libevrmod_radlinks.so`).
- **Kill switch.** If `/sdcard/readyatdawn/_local/evrmod.disable` exists, no plugin loads. A crash loop is fixed
  without reinstalling.
- **Build.** The loader's `build_quest.sh` (NDK r26d) → `build/libevrmod.so`.

## Uninstall

Delete `winmm.dll`, the `mods\` folder, and `winmm_orig.dll` if you made one, from `<game>\bin\win10\`. The
game then loads the real system `winmm.dll` as before. Nothing is written into the game directory except
`mods\evrmod.log` and the `mods\` folder itself.

## Defender caveat

An unsigned side-loaded `winmm.dll` can still be scored by Microsoft Defender (the reason the old injector was
quarantined). Before a test on your machine, confirm Defender's verdict on this file and, if needed, add a per-path
exclusion for your test install only. This is a research tool; do not ship it signed as anything it is not.
