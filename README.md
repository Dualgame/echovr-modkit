# echovr-modkit

Version 0.1.0 ([CHANGELOG.md](CHANGELOG.md)).

Client-side mods for the final releases of Echo VR, built for a LAN host running the EchoRelay fork
([Dualgame/EchoRelay](https://github.com/Dualgame/EchoRelay)). They finish two
features the game shipped but never backed: **invite codes** for the CREATE GAME / JOIN VIA CODE wizard, and the
**early-quit** notice and lockout.

> **Research / educational purposes only.** This project explores how a shipped game's frontend and services fit
> together. Use it on a LAN, with your own legally obtained copy of the game. It is not for hosting public matches,
> it is not affiliated with or endorsed by Ready At Dawn or Meta, and no support is provided.

## What is in it

| folder | what |
|---|---|
| `sdk/pc` | a `winmm.dll` proxy that loads `mods\*.dll` beside `echovr.exe`, and the hook broker (`include/evrmod.h`) that owns the game tick, `SendComponentEvent` and the metric-logger hooks once for every plugin |
| `sdk/quest` | the Quest APK builder: one ordered pipeline that patches **your own** stock store APK (boot fix, server choice from `/sdcard`, API host from config, early quit, the `libevrmod.so` loader, the login prerequisite), adds the plugins and re-signs with a key made on your machine |
| `sdk/quest/loader` | `libevrmod.so`, which loads every `libevrmod_*.so` plugin in the APK; no debuggable APK, no wrapper script |
| `mods/rad-links` | the invite-code plugin (PC DLL, Quest library): it watches the wizard, asks the host's `/invites` service for a code and joins by code |
| `mods/early-quit` | the PC plugin that restores the client half of the early-quit lockout in memory, and the Quest builder step that does the same in the game library |
| `mods/menu-cards` | puts CREATE GAME / JOIN VIA CODE cards on the main menu, generated from your own copy of the game's menu data |
| `docs/` | architecture, install pages, status, and the design lineage (how each feature compares with the designs it came from) |

## Requirements

This kit expects that you already have the right game files and know your way around them; it does not obtain,
back up or restore game files for you.

- **PC:** your own install of Echo VR build `6323983201049540` (2023-05-03), the final PC release. Other builds are
  refused: every patch checks the stock bytes it changes.
- **Quest:** your own copy of the Echo VR APK of build `6528386917200980` (version code 4987566, 2023-04-24) and its
  game data unpacked (the layout community installs use), and `adb`. Tested on a Quest 3 only. A re-signed APK has no Oculus social features (friends, party,
  store invites).
- **Services:** a host you connect to, such as the EchoRelay fork [Dualgame/EchoRelay](https://github.com/Dualgame/EchoRelay) (a derivative
  of [Xenomega/EchoRelay](https://github.com/Xenomega/EchoRelay); this release was tested against its tag `fork-2026-10`,
  see [docs/server.md](docs/server.md)) on your LAN with invites enabled. You fill in the
  client's `config.json` with that host's service addresses. The fork's `docs/protocol/` holds every wire format
  these mods speak.
- **To build:** see the platform table below. Python 3.10+ with `pip install -r requirements.txt` on whichever
  machine runs the Python tools.

| part | runs on | needs |
|---|---|---|
| PC proxy and plugins (`sdk/pc/build.cmd`, `sdk/pc/build_mods.cmd`, `mods/rad-links/build.cmd`) | Windows, a Visual Studio x64 developer prompt | MSVC x64 (Visual Studio 2022 Build Tools) |
| PC menu cards (`mods/menu-cards/menu_cards.py pc …`) | Windows | Windows Python with `C:/…` paths (or WSL `python3` with `/mnt/c/…` paths); an administrator prompt to write under `C:\Program Files` |
| Quest libraries (`mods/rad-links/build_quest.sh`, `sdk/quest/loader/build_quest.sh`) | Linux or WSL (bash) | Android NDK r26d (`linux-x86_64`) |
| Quest APK builder (`sdk/quest/quest_apk_build_release.py`) | Linux or WSL | Python, a JDK `keytool` (signing key) |
| Quest install and menu cards (`adb …`, `mods/menu-cards/menu_cards.py quest …`) | Linux or WSL | Python, `adb` (a Windows `adb.exe` also works from WSL) |

## Which build do I have?

These mods work only with the final release of each platform. Compare the sha256 of your file with the one below
(Windows: `certutil -hashfile echovr.exe SHA256`; Linux or WSL: `sha256sum`):

- **PC** build `6323983201049540` (2023-05-03): `bin\win10\echovr.exe`, 35,397,120 bytes, sha256
  `b6d08277e5846900c81004b64b298df6acba834b69700a640b758bda94a52043`. The plugins check the same executable by its PE
  header (timestamp `0x6452dff6`, image size `0x2231000`).
- **Quest** build `6528386917200980` (version code 4987566, 2023-04-24): the stock store APK, 96,177,060 bytes, sha256
  `21c7dc914dba2fa44f8daf019aedffa4a17e14186283ef05805217fc80d30eaf`. The APK builder refuses any other APK.

If your install kept updating after 2023-05, you have the farewell build (PC `6608609535920237` / Quest
`6848562085183460`, 2023-07-25). It has no game content, and these mods cannot use it.

## Getting started

1. Set up the host: the fork's `docs/headless.md`.
2. PC: [docs/install-pc.md](docs/install-pc.md). Quest: [docs/install-quest.md](docs/install-quest.md).
3. What has been tested, on which build: [docs/status.md](docs/status.md).

Nothing from the game is in this repository. The installers generate every modified game file on your machine, from
your own install; only hashes and offsets are committed. To return to stock, follow the uninstall steps on each
install page.

## What these mods change, plainly

- The PC proxy and plugins modify the running game in memory. The Quest builder modifies a copy of the game library,
  including a patch that bypasses the platform login prerequisite so the game can log in to your own host.
- Clients need these changes to use the features: a stock client cannot show an invite code or the early-quit notice.

## Credit

The menus, wizard pages and early-quit screens these mods use are DC Coffey's UI/UX work for Echo VR at Ready At Dawn
(case study: https://dccoffey.com/work/echo-vr-02). That work shipped inside the game but was never connected to a
service; this project only hooks up the functions behind it. See [docs/design-lineage/](docs/design-lineage/README.md).
The host side builds on [Xenomega/EchoRelay](https://github.com/Xenomega/EchoRelay).

## Continuing the work

Open items, known limits and where to start: [CONTINUING.md](CONTINUING.md). How the pieces fit:
[docs/architecture.md](docs/architecture.md).
