# echovr-modkit

Version 0.2.0 ([CHANGELOG.md](CHANGELOG.md)).

Client-side mods for the final releases of Echo VR, built for a LAN host running the EchoRelay fork
([Dualgame/EchoRelay](https://github.com/Dualgame/EchoRelay)). They finish two
features the game shipped but never backed: **invite codes** for the CREATE GAME / JOIN VIA CODE wizard, and the
**early-quit** notice and lockout. Version 0.2.0 adds the **Main Menu A** redesign and the plugin that makes it live.

## What 0.2.0 adds

- **Main Menu A** on PC (a language-package route: `build_main_menu_a.py pc`, then `pc-package`, `pc-apply`, and
  `pc-revert` to go back) and on Quest (the builder writes a language-package copy to push). On PC it cannot be
  combined with the menu cards.
- **Friends panel**: FRIENDS rows from the host, and a press that joins a joinable friend. **ADD FRIEND** stays the
  game's own: shown on Quest; on PC an opt-in (`card_add_friend=1`) that the game's card scripts switch off again, so it
  is off by default. The press goes to the platform's friends service, not to your host; a re-signed
  Quest APK has no Oculus social features, so ADD FRIEND does nothing useful there.
- **Season card**: the season slide and the HELP SEASON PASS tile open the season popup.
- **Report Player**: from the pause menu tile, the five-tile social card's REPORT tile, or the tablet (the stock LOBBY list → social card → REPORT →
  the REPORT PLAYER panel on the tablet, pause reasons page as fallback; the pause list scrolls); AI bots hidden from
  the list (`report_hide_bots`). Reports go to the host.
- **Quit warning** on the pause quit confirm in public lobbies, the **lockout lock** on PLAY / CREATE GAME / JOIN VIA
  CODE, the **first-time-user (FTUE) lock**, and the **HELP page**.
- **Invite link card**: the INVITE LINK page shows `<link_base>/deeplinks/launch?code=CODE`. The host sets the base
  (`--link-base`, default its LAN address); a client can override it with `invite_link_base` in its `config.json`. The
  host also serves a landing page at that link.
- **Host admin page** `/admin` on the fork: invites, their links, and reports filtered by player id (xplatformid) and
  reason. On the control port, or on the services port with `--admin-token`.
- **ECHO COMBAT card unlock** as a Quest builder step (`--no-combat-unlock` skips it). A combat match still does not
  start on Quest (the Quest build ships no combat level).

Host run line (the fork's `docs/headless.md` has the rest):

    EchoRelay.Headless --port 777 --storage-dir ./storage --game-dir "<Echo VR install>/bin/win10" \
        --invites true --friends true --reports true --early-quit true --control-port 7779 \
        [--link-base http://myhost.lan:777] [--admin-token TOKEN]

What was checked, per feature, platform and build: [docs/status.md](docs/status.md). A picture tour of the new pages:
[docs/menu-tour.md](docs/menu-tour.md).

> **Research / educational purposes only.** This project explores how a shipped game's frontend and services fit
> together. Use it on a LAN, with your own legally obtained copy of the game. It is not for hosting public matches,
> it is not affiliated with or endorsed by Ready At Dawn or Meta, and no support is provided.

## What is in it

| folder | what |
|---|---|
| `sdk/pc` | a `winmm.dll` proxy that loads `mods\*.dll` beside `echovr.exe`, and the hook broker (`include/evrmod.h`) that owns the game tick, `SendComponentEvent` and the metric-logger hooks once for every plugin |
| `sdk/quest` | the Quest APK builder: one ordered pipeline that patches **your own** stock store APK (boot fix, server choice from `/sdcard`, API host from config, early quit, the `libevrmod.so` loader, the login prerequisite, the ECHO COMBAT card unlock), adds the plugins and re-signs with a key made on your machine |
| `sdk/quest/loader` | `libevrmod.so`, which loads every `libevrmod_*.so` plugin in the APK; no debuggable APK, no wrapper script |
| `mods/rad-links` | the invite-code plugin (PC DLL, Quest library): it watches the wizard, asks the host's `/invites` service for a code, shows the code and the invite link, and joins by code |
| `mods/early-quit` | the PC plugin that restores the client half of the early-quit lockout in memory, and the Quest builder step that does the same in the game library |
| `mods/rad-menu` | the live half of the redesigned menu (PC DLL, Quest library): news carousel, friends panel, season card, Report Player (pause menu, social card, tablet, player card), ADD FRIEND on PC (opt-in, off by default), quit warning, lockout lock, FTUE lock, help page presses; options in its `radmenu.ini` |
| `mods/main-menu-a` | builds the redesigned main menu (Main Menu A) from your own install: a language-package copy on PC (`pc-package`, `pc-apply`, `pc-revert`) and on Quest (`quest`, push the pair) |
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
  of [Xenomega/EchoRelay](https://github.com/Xenomega/EchoRelay); this release was tested against its tag `fork-2026-11`,
  see [docs/server.md](docs/server.md)) on your LAN with invites enabled. You fill in the
  client's `config.json` with that host's service addresses. The fork's `docs/protocol/` holds every wire format
  these mods speak.
- **To build:** see the platform table below. Python 3.10+ with `pip install -r requirements.txt` on whichever
  machine runs the Python tools.

| part | runs on | needs |
|---|---|---|
| PC proxy and plugins (`sdk/pc/build.cmd`, `sdk/pc/build_mods.cmd`, `mods/rad-links/build.cmd`, `mods/rad-menu/build.cmd`) | Windows, a Visual Studio x64 developer prompt | MSVC x64 (Visual Studio 2022 Build Tools) |
| Main Menu A (`mods/main-menu-a/build_main_menu_a.py`) | `check`, `pc`, `quest`: Linux, WSL or Git Bash (needs `bash`). `pc-package`, `pc-apply`, `pc-revert`: Python only (Windows Python as administrator under `C:\Program Files`) | Python, `pip install -r requirements.txt` (zstandard, pillow); your own install |
| PC menu cards (`mods/menu-cards/menu_cards.py pc …`) | Windows | Windows Python with `C:/…` paths (or WSL `python3` with `/mnt/c/…` paths); an administrator prompt to write under `C:\Program Files` |
| Quest libraries (`mods/rad-links/build_quest.sh`, `mods/rad-menu/build_quest.sh`, `sdk/quest/loader/build_quest.sh`) | Linux or WSL (bash) | Android NDK r26d (`linux-x86_64`) |
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
3. The new menu: build and apply Main Menu A (`mods/main-menu-a/README.md`), then add `rad-menu` (`mods/rad-menu/README.md`).
4. What has been tested, on which build: [docs/status.md](docs/status.md).

Nothing from the game is in this repository. The installers generate every modified game file on your machine, from
your own install; only hashes and offsets are committed. To return to stock, follow the uninstall steps on each
install page.

## What these mods change, plainly

- The PC proxy and plugins modify the running game in memory. The Quest builder modifies a copy of the game library,
  including a patch that bypasses the platform login prerequisite so the game can log in to your own host, and a one-
  instruction patch that removes the lock on the ECHO COMBAT card (`--no-combat-unlock` skips it).
- Clients need these changes to use the features: a stock client cannot show an invite code or the early-quit notice.
- Main Menu A changes game data only (the menu levels and the resources they use), never the executable. On Quest it is
  a copy of the language package that you push to the headset and can put back; on PC the same copy is applied into
  your install by `pc-apply` (administrator rights under `C:\Program Files`) and put back by `pc-revert`. Both routes
  edit the language package, so they cannot be combined with `mods/menu-cards`.
- Reports are sent to your own host with the reported player's account id and display name. Host logs and the admin
  page show them, so keep both private.

## Known limits

- The Quest APK builder unlocks the ECHO COMBAT card by default (`--no-combat-unlock` skips it), but a combat match on
  the Quest does not start: the Quest build ships no combat level.
- On Quest the LOCKOUT tab and NEXT presses were checked by event only (no simulated touch reaches them; no real-hand
  press) (the tab itself shows the countdown and dims the card). The rest of the open items
  are in [docs/status.md](docs/status.md) and [CONTINUING.md](CONTINUING.md).

## Credit

The menus, wizard pages and early-quit screens these mods use are DC Coffey's UI/UX work for Echo VR at Ready At Dawn
(case study: https://dccoffey.com/work/echo-vr-02). That work shipped inside the game but was never connected to a
service; this project only hooks up the functions behind it. See [docs/design-lineage/](docs/design-lineage/README.md).
The host side builds on [Xenomega/EchoRelay](https://github.com/Xenomega/EchoRelay).

## Continuing the work

Open items, known limits and where to start: [CONTINUING.md](CONTINUING.md). How the pieces fit:
[docs/architecture.md](docs/architecture.md).
