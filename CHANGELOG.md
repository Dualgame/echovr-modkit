# Changelog

## 0.1.0 (2026-10-24)

The first release. Research / educational use, LAN only.

**Target builds** (every patch checks the stock bytes it changes and refuses any other build):

- PC `6323983201049540` (2023-05-03), the final PC release.
- Quest `6528386917200980` (version code 4987566, 2023-04-24), the final Quest release.

**Host:** validated against the EchoRelay fork [Dualgame/EchoRelay](https://github.com/Dualgame/EchoRelay) (a derivative of
Xenomega/EchoRelay), tag `fork-2026-10` = commit `6fe21f9ed19023b303d51b02db943f0bf2c40820`.

**What ships:**

- `sdk/pc`: the `winmm.dll` proxy that loads `mods\*.dll`, and the hook broker (ABI 2, `include/evrmod.h`) with its
  offline self-test.
- `sdk/quest`: the Quest APK builder (boot fix, server choice from `/sdcard`, API host from config, early quit, the
  `libevrmod.so` loader, the login prerequisite patch, plugins, v1 signing with a key made on your machine) and the
  `libevrmod.so` loader with its kill switch.
- `mods/rad-links`: invite codes for the CREATE GAME / JOIN VIA CODE wizard, PC DLL and Quest library: create, join by
  code, password prompt, region, and error messages in the wizard.
- `mods/early-quit`: the client half of the early-quit notice and lockout. PC: an in-memory plugin. Quest: a builder
  step. Plus a reference tool that patches a copy of the PC executable.
- `mods/menu-cards`: CREATE GAME and JOIN VIA CODE cards on the main menu, built from your own install (en_us only).
- `docs/`: architecture, install pages for PC and Quest, status, and the design lineage.

**Tested:** the install pages were followed on a clean install on 2026-10-24; each feature's result per build, and what
was not tested, is in [docs/status.md](docs/status.md). Open items: [CONTINUING.md](CONTINUING.md). Only a Quest 3
(Horizon OS 207) has been used.
