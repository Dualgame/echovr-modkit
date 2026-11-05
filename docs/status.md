# Status

## 0.2.0

Checked on 2026-10-31 and 2026-11-02 to 2026-11-05 against PC build `6323983201049540` (2023-05-03) and Quest build `6528386917200980` (version
code 4987566), with the host from [server.md](server.md) (the EchoRelay fork, tag `fork-2026-11`) on
the same LAN. Only a Quest 3 was used.

How to read the tables:

- **PASS** = seen working in this check. **FAIL** = run and did not work. **not yet tested** = built, and where noted
  checked offline, but not run on a client or device in this check; it may have worked in development, but that is
  not claimed here.
- **Three methods.** *event* = the press was sent to the running client as the UI event the button raises (the game's
  own handler runs after that, but nothing touched the button). *simulated touch* = a real collision: a hand that a
  test hook moves into the button, so the game's own touch code decides the press, on the PC client and on a Quest.
  *real* = a person's hand, finger or mouse. A row says which method it used; a cell with no method is *event*.
  *boot + picture* = the client was started and the screen looked at. No *real* press on a Quest was part of this
  check.
- A few rows say **no screenshot published**: the behaviour was seen in the client log, but no picture of it is in this repository yet.

### Main Menu A

| feature | PC `6323983201049540` | Quest `6528386917200980` |
|---|---|---|
| `build_main_menu_a.py check` on a stock install, then `pc` (58 files) / `quest` (60 rows) | PASS 2026-11-02, offline: the build reproduces the pinned tree file for file, `check` reports the install as stock and the build as 58/58 | PASS 2026-11-02, offline: the Quest pair built from the headset's stock data folder matches the pinned build |
| Main Menu A in the game from the built pair | PASS 2026-11-02 (boot + picture): `pc-package` then `pc-apply` into a clean install, no loader or loose-file mode; the home page is Main Menu A (cards, FRIENDS, the invite row); `pc-revert` then `check` reports the install as stock again. Repeated 2026-11-05 from the public commands on a clean copy: `pc-package`, `pc-apply`, then `pc-revert` leaves the stock install | PASS 2026-11-02 (boot + picture): the pair pushed to the headset shows Main Menu A. 2026-11-05: the pair from the public `quest` command is the tested pair byte for byte, pushed and reverted |
| rad-menu loads and stays inert on a stock menu (no Main Menu A data) | PASS 2026-11-05 (boot + log): the plugin finds no Main Menu A data and writes nothing | PASS 2026-11-05 (boot + log): same |
| news carousel and pager dots | PASS (event): carousel and dots draw, also on the Meta-account route 2026-11-05 | PASS 2026-11-05 (event): slide 2 shown |
| season card (slide, SEASON PASS tile, popup) | PASS (event); season and news popup pictures still owed | PASS (event); season and news popup pictures still owed |
| friends panel: rows drawn | PASS 2026-11-02 (boot + log): the plugin reports the FRIENDS rows present on the home page | not yet tested |
| friends panel: join from a row | PASS 2026-10-30 (event; a pointer press sent by a hook): the PC joined a Quest client's social lobby | not yet tested |
| ADD FRIEND on the five-tile social card | FAIL 2026-11-04 (picture): with `card_add_friend=1` the tile is not drawn, the game's card script switches it off again. Off by default now (`card_add_friend=0`); the stock card shows MUTE / GHOST / REPORT and INVITE TO PARTY | not yet tested (the stock script already shows the tile) |
| HELP page opens from the rail | not yet tested | PASS 2026-11-02 (event): the page draws with its eight tiles |
| HELP page tile presses | not yet tested | PASS 2026-11-02 (event): EARLY QUIT opens the early-quit page, STORE and REPORT ISSUE open the store page, LEGAL and CREDITS open theirs. TUTORIAL loads the tutorial. Not checked with a real hand |
| news tile click on slide 0 (the stock news page opens) | not yet tested | PASS 2026-11-02 (event) |
| FTUE lock (CREATE GAME / JOIN VIA CODE dimmed until the tutorial is done) | not yet tested | PASS 2026-11-02 (event, with `ftue=force`): CREATE and JOIN dim and their presses do nothing. The automatic mode (`ftue=auto`) was not tested |
| lockout lock: the LOCKOUT tab shows the countdown and the card dims while the host holds a lockout | PASS 2026-10-31 (host lockout set, tab counting `LOCKOUT 00:14:52`) | PASS 2026-11-02 (event): host lockout set, tab read `LOCKOUT 00:14:58`, card dimmed |
| lockout lock: pressing the LOCKOUT tab opens the early-quit page; NEXT on that page | PASS 2026-10-31 (event; a pointer press sent by a hook): the tab opens the early-quit page and NEXT opens page 2 | PASS 2026-11-02 (event): the tab's own event opens the early-quit page, and NEXT opens page 2. Not checked with a real hand |
| quit warning on the pause quit confirm in a public lobby; stock text in private and social lobbies | PASS 2026-10-31 (event): warning above YES / NO in a public arena match; private and social lobbies keep the stock text | social lobby keeps the stock text (2026-10-31); a Quest combat match cannot be started, so the warning is not tested there |

### Report Player

| feature | PC | Quest |
|---|---|---|
| pause menu REPORT: player list, reason, submit; the report reaches the host | PASS 2026-11-02 (event): reported a Quest player, host answered 201 and stored it with `entry=pause`, `platform=pc` | PASS 2026-11-02 (event): reported a PC player, host answered 201, stored with `entry=pause`, `platform=quest` |
| AI bots left out of the list (`report_hide_bots`) | PASS 2026-11-02 (event): two bots in an offline match, the list read "No other players to report." | not yet tested |
| tablet REPORT PLAYER panel, SUBMIT reaches the host (201, `entry=card`) | PASS 2026-11-03 (event) | PASS 2026-11-03 (event) |
| pause list scroll | PASS 2026-11-03 (event) | PASS 2026-11-03 (event) |
| tablet LOBBY row tap opens the player's card | PASS 2026-11-04 (simulated touch) | PASS 2026-11-04 (simulated touch) |
| five-tile card MUTE / GHOST / REPORT tiles | PASS 2026-11-04 (simulated touch) | PASS 2026-11-04 (simulated touch) |
| REPORT panel "<" back: one step to the reported player's card on the LOBBY list | PASS 2026-11-04 (simulated touch): "<" lands on the right card, twice in a row; 3 or more per path in later runs | PASS 2026-11-04 (simulated touch) |
| press on the top-right corner while the REPORT panel is shown: "<" takes it (the stock X under it is switched off), back one step, the tablet stays open | PASS 2026-11-04 (simulated touch) | PASS 2026-11-04 (simulated touch) |
| CANCEL on the reasons page: back one step to the card | PASS 2026-11-04 (simulated touch); no press goes through to the tab under it | PASS 2026-11-04 (simulated touch) |
| X on the plain LOBBY list closes the tablet, as stock | PASS 2026-11-04 (simulated touch) | PASS 2026-11-04 (simulated touch) |
| REPORT result screen: its X closes the tablet; OK returns to the LOBBY list | PASS 2026-11-05 (simulated touch) | PASS 2026-11-05 (simulated touch) |
| report flow on the Meta-account (OVR) route: Main Menu A and the tablet report | PASS 2026-11-05 (event) | not applicable |
| the tablet's own buttons are switched off while the REPORT panel is drawn (nothing under it can fire) | PASS 2026-11-05 (read back from the live buttons, plus simulated-touch presses): the stock tablet zones, the corner X included, are switched off under the REPORT panel and "<" takes that press | PASS 2026-11-05 (same) |
| pause menu REPORT list rows pressed with the laser pointer | not yet tested (the pointer cannot be moved by the test hook) | not yet tested |
| reporting the same player twice within ten minutes | PASS 2026-11-02 (event): the host answers 409 and the pause page and the tablet both show "Player already reported." | not yet tested |
| REPORT tile on the five-tile social card | PASS 2026-11-02 (event): host stored the report with `entry=card`, `platform=pc` | PASS 2026-11-02 (event): host stored the report with `entry=card`, `platform=quest` |
| Mute & Ghost box on the reason page | hidden by default (`report_mute_ghost=0`; the report sends `mute_ghost: false`). With `report_mute_ghost=1` the box is recorded in the report only and mutes or ghosts no one (use the social card's own MUTE and GHOST). Hidden: PASS 2026-11-02 (shot): no box, label or tick. Shown on the tablet panel (`report_mute_ghost=1`): PASS 2026-11-03 (event): ticked, the host stored `mute_ghost: true` | Hidden: PASS 2026-11-02 (shot) |

### Invites and the host

| feature | PC | Quest |
|---|---|---|
| INVITE LINK card shows `<link_base>/deeplinks/launch?code=CODE` | PASS 2026-11-02 (event): 53 characters, fits the card; the host's landing page answered 200 for the live invite | PASS 2026-11-02 (event): same text and length |
| link base: host `--link-base` and client `invite_link_base` overrides | PASS 2026-11-02 (event): the host default, the host's `--link-base` and the client's `invite_link_base` each gave the expected base. A 125-character link clips at both ends (about 64 characters show); keep the base short | the key is not yet tested on a client |
| the card's note line (replaced with "open the link to see the code, or enter it in JOIN VIA CODE") | PASS 2026-11-02 (event): fits on one line | not yet tested on a client |
| expired or closed invite on the landing page (410) | host unit tests only (host side) | not applicable (host side) |
| host `/admin` page and `/admin/data` | PASS 2026-11-02: control port page; services-port page answers 401 without the token and 200 with it; filter by account and by reason returned the reports above. No screenshot of the page is published | not applicable (host side) |
| host unit tests and the fork's local CI | PASS 2026-11-02 (69 unit tests; the .NET and native CI jobs, run locally by the maintainers on that date; the fork's README says how to run them) | not applicable (host side) |
| invite created through the wizard against a host with a registered game server | PASS 2026-11-02 (event): the host listed the invite with its link | not tested |
| the other invite flows (JOIN, password, region, revoke) | the 0.1.0 results below stand; not repeated for 0.2.0 | same |

### Quest builder

| feature | Quest |
|---|---|
| combat card unlock (step 10, `--no-combat-unlock` skips it) | PASS 2026-11-05 (boot + picture): a release APK built by the public builder with a new key shows ECHO COMBAT unlocked on CHOOSE EXPERIENCE on the headset; patch unit tests (13) and the dry-run also pass |
| clean install from the install pages: public builder output applied and reverted (PC), APK built with a new key and pushed (Quest) | PASS 2026-11-05 (PC and Quest) |
| combat match from the unlocked card | does not start a match: the ECHO COMBAT card can be chosen and the menu flow carries on, but the Quest data has no combat level, so no match starts. The exact screen after the failure was not recorded in this check; back out to the main menu or restart the game |
| `libradmenu.so` added as a plugin next to rad-links | builder row passes offline; the plugin itself ran in the headset test builds above |

### Known issues and gaps in this check

- After "<" the player's card occasionally closes again right after it re-opens (seen once in 12 PC presses); press the player's row to open it.
- Fixed 2026-11-02 (shot on a Quest): the right-hand footer of the pause menu drew its three items twice on Quest;
  the footer canvas now renders at its design size and draws once. PC checked, no change.
- Quest tab press: no simulated touch reaches the LOCKOUT tab press or the early-quit page's NEXT press, so they stay checked by event only (above); the HELP page tiles pass by event too.
- *simulated touch* is not *real*: no person pressed a button on a Quest in this check, and a Quest press with a real hand is the open item.
- The pause menu's laser pointer is not covered by simulated touch; the pause REPORT rows pass by event only.
- The three-player case of the large player card (which lobby row the stock MUTE / GHOST tiles act on) was not tested.
- Report Player, pause list: with hover scrolling, the list can step under the pointer just before you press, so the press picks the neighbouring player. The sentence on the next page names the player before SUBMIT; check it.
- Report Player, tablet: if a stock prompt replaces the report result prompt, pressing its OK can be taken as the report's OK and sends the tablet to the LOBBY list instead of the prompt's own action.
- Report Player, tablet: pressing a LOBBY row within about two seconds of "<" lets the back step continue, so other players' cards may open and close briefly. Wait a moment after "<" before pressing a row.
- Report Player, fault path: if the report flow hits an internal fault, the tablet's stock buttons can stay switched off until the level reloads. To recover, close and reopen the tablet, or rejoin the lobby.
- Screenshots for these checks are being retaken at full resolution; see [menu-tour.md](menu-tour.md).

Offline checks run for this release (no game, no device): the Report Player flow test (`mods/rad-menu/tests`, 669
checks, 0 failed; build and run it with the one-line command in that file's header), the invite-link text tests in `mods/rad-links/tests`, the combat-unlock patch tests, the fork's unit
tests, and the export gate over this repository's tree.

## 0.1.0

What was checked before this release, on a clean install that followed these docs (with the exceptions under
"Limits of this run"). Run on 2026-10-24 against
PC build `6323983201049540` (2023-05-03) and Quest build `6528386917200980` (version code 4987566), with the host
from [server.md](server.md) (fork tag `fork-2026-10` = commit `6fe21f9ed19023b303d51b02db943f0bf2c40820`; the run used an earlier commit with the same code, the later commits change only documentation and CI; invites on, early quit in
`enforce` mode) on the same machine.

**PASS** = seen working in this run. **not tested** = not run in this release check (it may have worked in
development, but that is not claimed here).

| check | PC | Quest |
|---|---|---|
| install from [install-pc.md](install-pc.md) / [install-quest.md](install-quest.md) on a clean install | PASS | PASS |
| boot and log in; the loader lists each plugin it loaded | PASS | PASS |
| main-menu cards: build, apply / push, then revert to the stock language package | PASS | PASS |
| CREATE GAME → CREATE INVITE CODE & LINK shows a code from the host | PASS | PASS |
| JOIN VIA CODE from the main menu joins that match | PASS | PASS |
| a PC joins a code created on a Quest (same match) | PASS (both) | |
| a Quest joins a code created on a PC | not tested | |
| early-quit lockout notice in the social lobby (lockout set by the host) | PASS | PASS |
| no early-quit plugin → no notice (control) | PASS | not tested |
| plugins alone, with a second plugin, and with `mods\load_order.txt` | PASS | — |
| plugin kill switch (`evrmod.disable`), also without storage permissions | — | PASS |
| without the install permissions (`-g`): the launch stops at Android's permission prompt, no login | — | as documented |
| update with the same key (`install -r -g`) keeps the game data | — | PASS |
| uninstall as documented leaves the game as it was | PASS | PASS |
| no service config on the headset: the stock hosts are only a fallback, no login | — | PASS |
| a second plugin loaded beside rad-links (both hook, in loader order) | PASS | PASS |
| password-protected match, region choice, AI teammates, wizard error pages, revoking a code, host restart with live codes | not tested | not tested |
| early quit counted from a real quit (rather than a lockout set by the host), and its timing after a match | not tested | not tested |

## Limits of this run

- Only a Quest 3 on Horizon OS 207 was used. Menu cards were checked in English only.
- The PC client ran windowed without a headset, with the EchoRelay patch DLL beside the game (it provides the windowed
  mode). A PC client in a headset was not part of this run.
- Quest pictures and checks were taken off-head; the test copy of the APK skipped the controller prompt.
- A re-signed Quest APK has no Oculus social features (friends, party, rooms, store invites).
- The first login shows the host's EULA and content panels.
