# Changelog

## 0.2.0 (2026-11-05)

The menu update. Everything below is a research / educational LAN tool, as in 0.1.0. Same target builds (PC
`6323983201049540`, Quest `6528386917200980`). What was checked and what was not: [docs/status.md](docs/status.md)
(checks of 2026-10-31 and 2026-11-02 to 2026-11-05).

**Host:** validated against the EchoRelay fork [Dualgame/EchoRelay](https://github.com/Dualgame/EchoRelay), tag
`fork-2026-11`. The features that talk to a host (friends, reports, the invite
link, the admin page) need that fork. Run line used for the checks:

    EchoRelay.Headless --port 777 --storage-dir ./storage --game-dir "<Echo VR install>/bin/win10" \
        --invites true --friends true --reports true --early-quit true --control-port 7779 \
        [--link-base http://myhost.lan:777] [--admin-token TOKEN]

**Added**

- `mods/main-menu-a`: Main Menu A, a redesigned main menu built from your own install: an ONLINE / SETTINGS / HELP /
  QUIT rail, PLAY / CREATE GAME / JOIN VIA CODE cards, a FRIENDS column, a news carousel, a HELP page with eight tiles,
  a home button on every tab, and a redesigned pause menu (mute / ghost / report tiles with CREATE GAME and
  JOIN VIA CODE cards). `build_main_menu_a.py check` verifies your install against the build's pins and writes nothing.
  Menu design by DC Coffey.
  - **PC:** a language-package route. `pc` builds the verified resource tree (58 files); `pc-package` appends it as
    rows to a copy of your own language package; `pc-apply` puts that copy into the install; `pc-revert` puts your
    stock copy back. No loader or loose-file mode is involved. It cannot be combined with `mods/menu-cards` (both edit
    the same package; revert the cards first).
  - **Quest:** `quest` builds a copy of the language package to push to the headset, and the stock pair is your revert.
- `mods/rad-menu`: the plugin that makes the new menu live (PC DLL, Quest library, offline test):
  - the news carousel with pager dots, and the **season card**: the season slide and the HELP SEASON PASS tile open the
    season popup (the fork carries the Season 7 content);
  - the **friends panel**: FRIENDS rows with names and status from the host, and a press that joins a joinable friend;
  - **ADD FRIEND** on the five-tile social card on PC is an opt-in, off by default (`card_add_friend=1`): the game's own
    card scripts switch the tile off again, so it flickers and is mostly not drawn (on Quest the stock script shows it). The press goes to the platform's own friends service, not to the host, and the host's friends list is separate (a
    re-signed Quest APK has no Oculus social features);
  - **Report Player** from the pause menu, the social card or the tablet: pick a player, one of six reasons, optional
    mute and ghost (recorded in the report), submit to the host. The pause menu shows the REPORT tile and its popup
    pages (player list, reasons, SUBMITTED); the social card gets a fifth tile, REPORT; on the tablet, the stock
    LOBBY list (its own scroll, no "more players" rows) opens the card, and its REPORT opens the REPORT PLAYER panel on the tablet
    (reasons, Mute & Ghost option, SUBMIT; `report_tablet_reasons`; "<" and CANCEL go back one step to the LOBBY list
    with the player's card open, the X on the LOBBY list still closes the tablet, `report_tablet_cancel=list` lands
    CANCEL on the list with the card closed), with the pause reasons page as the automatic fallback; the pause list
    scrolls (five rows and a peek row). AI bots are left out of the list
    (`report_hide_bots`, on by default). Each report carries the target's name, the entry point (`card`, `pause` or
    `tablet`) and the platform;
  - the **quit warning** on the pause quit confirm in public, non-social lobbies (on by default);
  - the **lockout lock**: PLAY / CREATE GAME / JOIN VIA CODE show a countdown and open the lockout notice while the
    early-quit lockout is live (`lockout_lock=auto`, the default);
  - the **FTUE lock**: CREATE GAME / JOIN VIA CODE dim until the tutorial is done (`ftue=auto`, the default);
  - the **HELP page** presses (SEASON PASS, SAFE ZONE); the page itself is part of Main Menu A.
- **Back one step on the tablet.** In the REPORT PLAYER panel, "<" (in the X's corner) and CANCEL go back one step to
  the reported player's card on the LOBBY list (the card is re-opened directly and checked). The X on the plain LOBBY
  list still closes the tablet. While the REPORT layer is drawn, the stock tablet buttons under it are switched off
  by the engine, so a press cannot go through to a tab or the X, and for one second after the closing press they stay
  consumed.
- **Simulated-touch checks.** The tablet row, card and back-one-step presses were verified with a real collision from
  a hand moved by a test hook, on PC and Quest (see [docs/status.md](docs/status.md)); a real hand on a Quest is still
  not checked.
- **Invite link card.** The invite wizard's INVITE LINK page shows `<link_base>/deeplinks/launch?code=CODE` instead of
  the stock placeholder. The host owns the link base (`--link-base`; default: the host's LAN IPv4 address (first private-range address, real adapters before virtual ones, logged at
  start-up; set `--link-base` if headsets cannot reach it) and its services port) and returns the finished link with each invite; a client can override the base with
  `invite_link_base` in its `config.json`; with neither, the page builds the link from `apiservice_host`. The page's
  note line now reads "open the link to see the code, or enter it in JOIN VIA CODE".
- **On the fork:**
  - `/deeplinks/launch?code=CODE`: a read-only landing page for an invite (live, expired or closed);
  - `/admin`: a page that lists invites, their links and reports. Reports are filtered by player id (the
    xplatformid) and by reason. It is on the control port (loopback), or on the services port when the host is started
    with `--admin-token` (the token as `Authorization: Bearer` or `?token=`); the same data is served as JSON on
    `/admin/data`, and `GET /reports` takes the same two filters;
  - reports now store the target's name, the entry point and the platform; invite replies carry `link_url`.
- **Quest builder:** the ECHO COMBAT card unlock is step 10 and on by default (`--no-combat-unlock` skips it). It only
  removes the lock on the CREATE GAME page; see the known limits. The builder also adds `libradmenu.so` as a plugin
  next to rad-links when it is built.
- `sdk/pc/build_mods.cmd` builds `rad-menu.dll` with the other plugins.

**Changed**

- The `mods/main-menu-a` build is pinned to the current menu chain (58 files on PC, 60 rows on Quest); the earlier
  39-file pin is gone.
- rad-menu defaults for a release: `lockout_lock=auto`, `quit_warning=1`, `ftue=auto`, `combat_lock=off`,
  `report_hide_bots=1`, `report_tablet_reasons=ac`, `report_tablet_cancel=card`, `card_add_friend=0`,
  `report_mute_ghost=0`.

**Known limits**

- A Quest combat match does not start: the Quest build ships no combat level. The combat card unlock only unlocks the
  card, and the quit warning in combat cannot be seen on Quest for the same reason.
- On Quest the LOCKOUT tab and NEXT presses were checked by event only (no simulated touch reaches them; no real-hand
  press). All eight HELP tiles were pressed by event on Quest; TUTORIAL loads the tutorial.
- Mute & Ghost on the report is recorded in the report only.
- English only.
- The Quest check of 2026-11-02 used an event-driven press for the new pages; a finger press was not checked. See
  [docs/status.md](docs/status.md) for every row.

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
