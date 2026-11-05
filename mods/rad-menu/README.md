# rad-menu

The live half of the redesigned main menu. [`mods/main-menu-a`](../main-menu-a/README.md) builds the menu's data (new
pages, cards, tiles) from your own install; rad-menu is the plugin that fills them with life: the news carousel,
the FRIENDS rows, the tab highlight, Report Player (pause menu, social card, tablet), ADD FRIEND on
PC (opt-in), the quit warning, the first-time-user lock, the early-quit
lockout lock, and the HELP page presses. One source file per platform (`src/radmenu.c` for the PC DLL,
`src/radmenu_quest.c` for the Quest library), one shared Report Player flow (`src/report_flow.h`).

The menus and screens it drives are DC Coffey's UI/UX work for Echo VR at Ready At Dawn (case study:
https://dccoffey.com/work/echo-vr-02). rad-menu only connects them to a host.

Targets: PC build `6323983201049540` and Quest build `6528386917200980` (version code 4987566). Any other image is
refused; every function it calls has its first bytes checked at start-up and a mismatch leaves the plugin inert.
What has been tested, on which build: [docs/status.md](../../docs/status.md).

## What it does

| feature | what you see | needs from the host |
|---|---|---|
| news carousel | the news tile cycles three slides every 5 s (self-goaling patch, season 7 launch, early-quit info) with pager dots; the season slide opens the season popup | nothing |
| tab highlight, footer copy | the active rail tab (ONLINE / SETTINGS / HELP) is filled; the banner follows the tab; empty FRIENDS rows are hidden | nothing |
| FRIENDS rows | the friends column shows names and status; pressing a row with a joinable match joins it (through rad-links) | the fork with `--friends true` |
| HELP page | the SEASON PASS and SAFE ZONE tiles open their pages | nothing |
| Report Player | from the pause menu tile, the social card's REPORT tile or the tablet: pick a player, a reason, submit; on the tablet the stock LOBBY list (its own scroll) → social card REPORT → the REPORT PLAYER panel on the tablet (reasons grid, CANCEL / SUBMIT; the pause reasons page is the automatic fallback; the X in the top-right corner still closes the tablet; on the panel that corner shows "<" (back), and "<" or CANCEL goes back one step to the LOBBY list with the player's card open; OK on the result screen returns to the LOBBY list); the pause list scrolls (five rows and a peek row, no "more players" rows); AI bots left out of the list | the fork with `--reports true` (`POST /reports`) |
| ADD FRIEND (PC, off by default) | with `card_add_friend=1` the five-tile social card's ADD FRIEND tile is enabled and shown on each card open (on Quest the stock script already shows it). Off by default: the game's own card scripts switch the tile off again under it, so on PC it flickers and is mostly not drawn. The press goes to the platform's friends service, not to your host | nothing |
| quit warning | the pause quit confirm shows a one-line warning in public non-social lobbies | nothing |
| FTUE lock | CREATE GAME / JOIN VIA CODE are dimmed ("Complete the tutorial first") while the picker shows them for a new player | nothing |
| lockout lock | PLAY / CREATE GAME / JOIN VIA CODE are locked with a countdown while the early-quit lockout is live, and the lockout notice opens on a press | the early-quit patch (`mods/early-quit`) and a host that sets a lockout |
| combat card | optional grey lock on the combat card (`combat_lock`) | nothing |

All writes need the Main Menu A tree: the news tile, banner and rail buttons are stock actors, so nothing is written
unless the tree's marker actor is in the menu gamespace. On a stock menu the plugin only tracks pages (log line
`tree marker absent`). The Report Player rows come from the net game in process (the lobby's users); only when that
reader is unavailable does the plugin ask the host's `GET /reports/players`.

## Requirements

- Echo VR PC `6323983201049540` or Quest `6528386917200980`, with the Main Menu A data applied.
- The broker (`sdk/pc`, ABI 2) on PC; the `libevrmod.so` loader (`sdk/quest`) on Quest.
- `mods/rad-links` for the join-from-friends press.
- To build: MSVC x64 (Visual Studio 2022 Build Tools) on Windows for the DLL; Android NDK r26d on Linux or WSL for
  the Quest library.

## Build and install

PC (from a Visual Studio x64 developer prompt):

    sdk\pc\build_mods.cmd            builds every plugin into sdk\pc\build\mods\, rad-menu.dll among them
    mods\rad-menu\build.cmd          or this one alone: mods\rad-menu\build\rad-menu.dll

Copy `rad-menu.dll` into the `mods\` folder beside `winmm.dll` (see [docs/install-pc.md](../../docs/install-pc.md)).
The log is `_local\radmenu.log` beside `echovr.exe`.

Quest:

    mods/rad-menu/build_quest.sh     writes mods/rad-menu/build/libradmenu.so   (NDK=... overrides the NDK path)
    python3 sdk/quest/quest_apk_build_release.py --apk STOCK.apk --out OUT.apk --keystore my.p12

The APK builder adds `libradmenu.so` as `libevrmod_radmenu.so` next to rad-links when it is built (and says so when it
is not). Pass `--plugin` to choose plugins by hand. The Quest log is logcat tag `radmenu`. The kill switch for every
plugin is the loader's: create `/sdcard/readyatdawn/_local/evrmod.disable`.

Offline test of the Report Player flow (no game, no device):

    cc -std=c11 -Wall -Wextra -o trf mods/rad-menu/tests/test_report_flow.c && ./trf

## Options (`radmenu.ini`, `[radmenu]`)

PC: `<game>\bin\win10\_local\radmenu.ini`, beside the exe. Quest: `/sdcard/Android/data/com.readyatdawn.r15/files/radmenu.ini` (the library also tries `/sdcard/radmenu.ini`); push it with

    adb shell mkdir -p /sdcard/Android/data/com.readyatdawn.r15/files
    adb push radmenu.ini /sdcard/Android/data/com.readyatdawn.r15/files/radmenu.ini

The file is read when the game starts. Plain
`key=value` lines; `#` and `;` start comments. An absent file gives the defaults below, and the same keys and
defaults apply on both platforms. Unknown keys are logged and ignored. [`radmenu.ini.sample`](radmenu.ini.sample)
lists the common ones.

| key | default | what it does |
|---|---|---|
| `enabled` | 1 | 0 installs no hook at all |
| `carousel` / `tab_title` | 1 / 1 | 0 stops the news tile and dot writes / the banner write |
| `period_ms` | 5000 | slide period (PC min 1000, Quest min 500) |
| `sprite` | 1 | slide 1 shows the season tile art |
| `dots`, `dot_glyph` | 0, `o` | 1 = ASCII suffix, 2 = second line, 0 = off; text glyph for the pager dots |
| `fixed` | -1 | 0..2 holds one slide |
| `hover_pause`, `hover_resume_ms` | 1, 2000 | rotation holds while the pointer is on the tile or a dot |
| `friends`, `friends_period_ms` | 1, 5000 | FRIENDS rows poll (min 1000) |
| `user_id` | empty | empty = taken from the game's own metric lines |
| `tab_highlight` | 1 | 0 = old "> X <" marker, 2 = ShowUIElement fallback |
| `square_dot`, `help_season`, `season_p24_first` | 1, 1, 0 | square pager dot; HELP SEASON PASS route; show the home page first when the news page is off |
| `season_comp` | `0x6aea99eeb2bab6c4` | component for the built season event |
| `help_safezone`, `help_safezone_card` | 1, 1 | HELP SAFE ZONE routes |
| `footer_copy`, `hide_empty_rows` | 1, 1 | ONLINE footer strings; hide FRIENDS rows with no friend |
| `ftue` | `auto` | `off`; `auto` = dim CREATE / JOIN ("Complete the tutorial first") when the home picker enables them; `force` = every load |
| `combat_lock` | `off` | `purchase` / `hardware`: the combat card is grey with the stock lock text, the press is consumed |
| `lockout_lock` | `auto` | `off`; `auto` = lock PLAY / CREATE / JOIN while the early-quit lockout is live. It first reads the three early-quit patch sites (an unrecognised exe or library never locks), then the net game's early-quit flags and expiry; a stock exe or library stores no lockout, so it never locks. `force` = a synthetic countdown |
| `lockout_force_sec`, `lockout_text` | 900, `LOCKOUT` | `force` countdown (5..86400 s); card label prefix |
| `quit_warning` | 1 | warning text on the pause quit confirm, public non-social lobbies only (private and `social_2.0*` lobbies stay stock) |
| `quit_warning_text`, `quit_warning_element`, `quit_warning_actor` | built-in line, `0x0ba8656934c17e91`, scan | one unwrapped line (about 60 characters); element; pin the widget actor |
| `report` | `auto` | `auto` acts only while the Main Menu A popup pages are loaded; `1` / `on` any level; `0` / `off` |
| `report_tile_uid` | `0xaa15d7d322666e42` | pause REPORT tile; 0 = no pause entry |
| `report_card_event`, `report_card_uid`, `report_card_element`, `report_card_glyph` | learn, 0, per-platform label, `cancel_button` | social card REPORT button: press event, element, label, sprite region (empty glyph = off) |
| `card_add_friend` | 0 | off by default (on PC the game's card scripts switch the tile off again, so it flickers). `1` = PC: enable and show the ADD FRIEND tile of the five-tile social card on each open (the engine call is checked against the known executable first; a mismatch logs "ADD FRIEND disabled" and calls nothing); `2` also re-applies every second (fallback if a reopen of the same player raises no card event); `0` off. Quest: no effect, the stock script already shows it |
| `report_list_scroll` | `auto` | how the pause REPORT player list scrolls: `auto`, `stick`, `hover` or `off` |
| `report_tablet_reasons` | `ac` | the social card's REPORT on the tablet: `ac` = the REPORT PLAYER panel on the tablet itself (reasons grid, CANCEL / SUBMIT; taken as `popup` when its screen is not installed); `popup` = the pause reasons page with the player already picked (the pause menu opens; CANCEL / OK close it); `field` = one tablet page [Report <name>:, Select Reason > (each press steps through the six reasons), SUBMIT, CANCEL], also used when the popup pages are not loaded |
| `report_tablet_cancel` | `card` | where the tablet REPORT PLAYER panel's CANCEL goes: `card` = one step back, as "<": the LOBBY list with the reported player's card open; `list` = the LOBBY list with the card closed. "<" always goes back to the card |
| `report_hide_bots` | 1 | AI bots are left out of the report player rows (they have no account) |
| `report_popup`, `report_popup_event` | 1, `evt_btn_clicked` | pause popup pages (only when the Main Menu A popup pages are loaded); their press event |
| `report_card5_event`, `report_card5_uid`, `report_card5_learn` | `delegate_OnFullyDepressed`, `0x2fd59a8b4a7b59df`, 1 | five-tile card REPORT button; log its events for 60 s after each card open |
| `report_mute_ghost` | 0 | 0 = no Mute & Ghost box on any entry (the report sends `mute_ghost: false`); 1 = the box is shown on the reason page and recorded in the report only (it mutes or ghosts no one) |
| `report_fake_rows` | 0 | test only: n synthetic players appended, never sent |

## Report Player

Entry points: the social controls card (a REPORT tile; with Main Menu A the card has five tiles on Quest; on PC the stock card hides ADD FRIEND, see `card_add_friend`),
the pause menu's REPORT tile and its popup pages, or the tablet's player list (optionally through the large player
card). The flow is: pick a player, pick one of
six reasons, optionally MUTE & GHOST (recorded in the report only), SUBMIT. The result prompt is SUBMITTED, ALREADY
REPORTED (the host answered 409) or NOT SENT.

On the tablet the top-right corner holds the game's X (close the tablet) on the LOBBY list and the card, and "<"
(back) on the REPORT PLAYER panel. They share the corner, not the job: "<" goes back one step, to the LOBBY list with
the reported player's card open, and CANCEL does the same (`report_tablet_cancel=list` makes CANCEL land on the list
with the card closed). The X on the LOBBY list still closes the tablet.

The report is one `POST /reports` to the services host (`apiservice_host` in `config.json`), JSON:

| field | meaning |
|---|---|
| `target_user_id`, `target_name` | the reported account (numeric id) and the display name as the game shows it |
| `reason` | one of the six reasons |
| `mute_ghost` | the checkbox |
| `session_id`, `match_type` | the lobby UUID and its match type, read in process |
| `user_id`, `platform` | the reporter (optional) and `pc` or `quest` |
| `entry` | `card`, `pause` or `tablet` (omitted when the opener is unknown) |

The Mute & Ghost box is recorded in the report only: it does not mute or ghost the player. Use the social card's own
MUTE and GHOST for that.

The fork's `/admin` page lists the stored reports and filters them by player id (xplatformid) and reason (see
[docs/server.md](../../docs/server.md)). The
host keeps what it is sent; rad-menu logs names as `#xxxx` and ids as their last four digits.

## QUIT WARNING

The stock quit confirm on the pause menu already has a second line for arena and combat that stock never fills. On each
pause-menu open the plugin reads the lobby in process: a public, non-social lobby gets the one-line warning (default
"Quitting early affects everyone and adds a penalty. Quit?"); private and `social_2.0*` lobbies, an unverified build, or
no lobby keep the stock text.

## Lockout lock

With `lockout_lock=auto`, PLAY / CREATE GAME / JOIN VIA CODE go grey with the lock icon and `LOCKOUT hh:mm:ss`,
refreshed every second, while the early-quit lockout is live. A press opens the stock lockout notice with a one-line
body and the timer. When the lockout ends or the host clears it, the cards return to their normal state. Only a client
with the early-quit patch (`mods/early-quit`) ever has a lockout; a stock client never locks.

## Log lines

`ready: ...` and `tree marker present|absent` at start; `page 24 enabled`; `carousel on`; `slide change 0 -> 1`;
`friends: GET /friends ... -> 200`; `report: rows from session reader: ...`; `QUIT WARNING: ...`;
`LOCKOUT state: locked|none ...`. The PC log rolls to `radmenu.log.1` at 256 KB.

## Limits

- Only the English menu strings are tested.
- On Quest the LOCKOUT tab and NEXT presses were checked by event only (no simulated touch reaches them; no real-hand
  press); the HELP page tile presses were checked by event too. What was checked: [docs/status.md](../../docs/status.md).
- The FRIENDS rows, Report Player and `/admin` need the fork; against another host the plugin keeps the build-time
  texts and backs off (300 s, doubling to 30 min after 400 / 404 / 405 / 409 or a transport error).
- A Quest combat match does not start (the Quest build ships no combat level), so the quit warning is not shown there
  in combat.
- A re-signed Quest APK has no Oculus social features; the FRIENDS rows are the host's, not Oculus's.

MIT (see LICENSE).
