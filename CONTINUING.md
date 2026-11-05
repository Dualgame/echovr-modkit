# Continuing the work

Open items, known limits and where to start, for the client side: the mod loader, the plugins (rad-links, early-quit, rad-menu), the Quest APK builder,
Main Menu A and the menu cards. Host-side items (invite auth, early-quit enforce mode, game-server work, server density) are in the
EchoRelay fork's `CONTINUING.md`. This is a research / educational project for LAN use; it does not host public matches.

Every item has an **entry point** (a file and function in this repository, or a virtual address with the build it
belongs to) and an **acceptance test**. Addresses are valid only for PC `6323983201049540` and Quest
`6528386917200980` (version code 4987566). Source names are relative to this repository: `mods/rad-links/src/radlinks.c`
(PC) and `mods/rad-links/src/radlinks_quest.c` (Quest) share no code.

## Known limits

- **A malformed Quest config leaves the Quest offline.** The Quest reads its server choice from
  `/sdcard/readyatdawn/_local/config.json` (a stub the APK builder patches into the game library). If the file is
  absent, the client uses the host baked into the APK. If it is present but is not valid JSON, the client reports a
  JSON error and stays offline: no fallback, no crash. Delete the file to recover.
- **The Quest API host follows the config key `apiservice_host`**, and falls back to the host baked into the library
  when the key is absent. rad-links resolves host names on both platforms; on Quest it takes the host from the
  engine's config at login.
- **Invites are address-bound, not token-bound**, and early quit runs in shadow mode by default (it records, refuses
  nothing). Both are host properties: see the fork's `docs/protocol/invites.md` and `docs/protocol/early-quit.md`.
- **Stock clients** show a fixed "SERVER IS LOCKED" text for a refused request, never the host's message.
- **No Oculus social features on a re-signed Quest APK** (friends, party, invites through the store).
- **Only a Quest 3** (Horizon OS 207) has been tested. Main-menu cards exist for en_us only.
- **The wizard's password step:** the field keeps the 5-character code, so the player presses CLEAR before typing the
  password (item 2).
- **The broker is PC-only.** On PC, `winmm.dll` owns the tick, `SendComponentEvent` and metric-logger hooks once and
  hands them to plugins (ABI 2, `sdk/pc/include/evrmod.h`). On Quest, `libevrmod.so` only loads the
  `libevrmod_*.so` plugins; each plugin still hooks for itself. Kill switch: create
  `/sdcard/readyatdawn/_local/evrmod.disable`.
- **Quest rad-links log:** it writes `files/radlinks.log` in the app's private storage, which a release (non-debuggable)
  APK cannot read back. Use `adb logcat -s radlinks:V` instead.

## Open items

### Invite codes (client)

| # | item | entry point | acceptance |
|---|---|---|---|
| 1 | **In-lobby wizard.** Step 1: a typed-code join from a social lobby. Step 2: a create from the in-lobby menu. The in-lobby menu master already ships the wizard pages (8–14) and its pause page script already routes the two cards (CREATE → page 9, JOIN → page 8); the two card actors are on no page. | `mods/menu-cards`: put the two shipped, page-less card actors on page 1 of `mnu_master_mp_ingame` (PC) / `mnu_master_mp_ingame_lowspec` (Quest; same bytes). rad-links `on_wizard_event` and `join_link`: accept the in-lobby page owners (page 8 `0xb64cc26afecf9254`, page 13 `0x88601e738c5290fb`, page 14 `0xd32ed3afc191ad71`). PC join routine `0x140178700`, leave routine `0x1401796b0` (leave first when in a match). Quest: `CR15NetClientLobby::Join` `0x12462a8`. | Step 1: a client in a social lobby types a code, resolves it and ends in the invite's session, on PC and Quest. Step 2: a create from inside a lobby produces a code and a second client joins with it. If it stalls, record the owners that were not seen. |
| 2 | **Password prompt without CLEAR.** The join works when the player clears the field first. | rad-links `p34_text`, `on_wizard_event`: clear the field when ENTER PASSWORD is shown. Also read the field's maximum length and whether the create and join pages share a character set. | PC creates with a password; the Quest types the code, then the password with no CLEAR press, and joins. |
| 3 | **A non-default region from the wizard.** The default (US WEST) is tested end to end. | The wizard's region row (page 40 on the main menu) and the `region` field rad-links sends to the host's `POST /invites`. | A create with EU WEST lands on a `euw` server. |
| 4 | **Link that joins.** Built in 0.2.0 as a link that shows the invite: the INVITE LINK page shows `<link_base>/deeplinks/launch?code=CODE` and the host's landing page shows the invite's state. The link does not start the game. | rad-links `link_view`, the fork's `/deeplinks/launch`. | A second machine opens the link and ends up in the match (needs item 5). |
| 5 | **Outside links and an installer.** A protocol handler or launcher that resolves a code and starts the game. | A new launcher: `GET /invites/{code}`, then start `echovr.exe -gametype <g> -lobbyid <link>`. | Opening a code link from outside the game starts the client and joins; the installer removes cleanly. |
| 6 | **Quest rows not run:** COMBAT PAYLOAD FISSION invites, and the lobby flow on Quest. | rad-links Quest library; the invite flows of the fork's `docs/protocol/invites.md`. | Both complete with log evidence on a Quest 3. |
| 7 | **Quick / Advanced Invite and a party join policy.** Design-only (not in any shipped build). | New canvases, plus the host's `player_limit` / `settings` create fields. Party semantics need the host's friend and party messages (fork `CONTINUING.md`). | A host-enforced party policy visible in the invite flow, or a recorded decision not to build it. |

### Early quit (client)

| # | item | entry point | acceptance |
|---|---|---|---|
| 8 | **Ribbons and titles.** Never seen on a device. | The tier index is in the Update message (offset `0x35`) and the `steady_player_levels` table. Client side: the scoreboard ribbon and the title strings, the podium matchmaking-interface canvas and the post-match progression page. | A device screenshot of a ribbon on the scoreboard and the earned title for at least one tier, on the patched client. |
| 9 | **Progress meter.** The canvas that holds it is not identified. | Search the loyalty readout widgets (the placeholders `QUIT: 999% / COMPLETE: 999%` and `LOYALTY TIER 0`). | The meter is identified, driven by the Update's counts, and shown on a device. |
| 10 | **Quit warning on the pause quit confirm (YES / NO).** Built in 0.2.0: rad-menu fills the stock warning line (`quit_warning`, on by default) in public, non-social lobbies. PC: checked 2026-10-31 (event); the Quest social lobby keeps the stock text (2026-10-31); not visible on Quest in combat (no combat level). | `mods/rad-menu` (`quit_warning`, `quit_warning_text`). | Pressing quit in a public arena match on PC and on Quest shows the warning above YES / NO, and private and social lobbies keep the stock text. |
| 11 | **"Warning seen" flag and tier labels.** The host never reads the client's seen bit; the label per tier index is unconfirmed. | `mods/early-quit`: PC sites (build `6323983201049540`) `0x1401618b7`, `0x1401ae2f0`, `0x1401ae372`, `0x140d96319`; Quest sites (build `6528386917200980`) `0x126ac3c`, `0x126ac70`, `0x126acb0`, `0x2321ce0`, `0x2321ce4`, `0x2321cf0`. | A recorded decision, or a device run showing the seen bit round-trips and each tier index shows the right label. |

### Menus and frontend

| # | item | entry point | acceptance |
|---|---|---|---|
| 12a | **Report Player edge cases.** Hover scrolling can move the pause list under the pointer so a press picks the neighbouring player; a stock prompt replacing the tablet result prompt has its OK taken as ours; a LOBBY row pressed within about two seconds of "<" lets the back step open and close other cards; after a fault the tablet's stock buttons stay off until the level reloads. | `mods/rad-menu/src/report_flow.h`. | Each is fixed (lock the target when the pointer enters, clear stale prompt state, stop the back step when the user acts, restore the buttons on a fault) and checked on a client. |
| 12 | **Future Frontend remainder, Report Issue.** Main Menu A (the rail, FRIENDS, carousel, HELP page) and Report Player (reports to the host, `/admin` viewer) are built in 0.2.0. Left: the PROFILE and CONTROLS tabs, an INVITE FRIENDS panel, Report Issue (the help popup's link is not traced), applying the Mute & Ghost box through the social card, a real hand check of the Quest LOCKOUT tab and early-quit NEXT presses (checked by event only; no simulated touch reaches them), a real hand press check on Quest (simulated-touch verdicts are in status.md), the pause menu laser-pointer presses (the test hook cannot move the pointer), and ADD FRIEND on the PC social card: rad-menu's enable (`card_add_friend`, now off by default) loses to the game's own card scripts, which switch the tile off again, so it flickers and is mostly not drawn; a working tile needs the tile held on for as long as the card is open, or a decision to leave it Quest-only. | See the design-lineage pages. | Each: a canvas or press that works in game and a destination that receives the action, or a recorded decision not to build it. |
| 13 | **Other languages.** Cards and Main Menu A exist for en_us only. | `mods/menu-cards`: the language-package row for the cards and the page-data patch. | The cards appear and read correctly with the client set to each other shipped language. |

### Loader, SDK and platforms

| # | item | entry point | acceptance |
|---|---|---|---|
| 14 | **ABI 3 and the SDK split.** A Quest broker (one owner of the hooks, as on PC), a byte-patch primitive (so PC early quit stops patching in `DllMain`), an incoming-SNS hook, and config / log services. | `sdk/pc/include/evrmod.h`, `sdk/pc/src/winmm_proxy.c`, `sdk/quest/loader/evrmod_loader.c`. Split `sdk/` into its own repository only after the Quest loader ships under the same ABI, one release passes with no breaking ABI change, and a plugin outside this repository builds against it. The mods stay here until then: the Quest half of early quit is a builder step (a `libr15.so` patch), not a plugin, and the mods share one ABI and one install. | rad-links and early quit run under the ABI 3 broker on both platforms with the same plugin source. |
| 15 | **Quest rad-links log in shared storage.** | `radlinks_quest.c`: the log path. Move it to `/sdcard/r14logs/` (where the game writes its own logs) or drop the file and keep logcat. | A release APK's rad-links log is readable with `adb pull`. |
| 16 | **Quest 2 and Quest 3S.** Only a Quest 3 has been tested. | The Quest patches are tied to the build, not the device; check graphics-driver differences and the boot fix first. | The full install and invite flow on each device, with a stated OS version. |
| 17 | **A one-step Quest installer.** Today the install is the builder plus a few `adb` commands. | A small wrapper around `sdk/quest/quest_apk_build_release.py`: take the user's APK and data folder, build, uninstall a differently signed copy, `install -g`, push the data folder and `config.json`, and optionally the cards. | One command from a stock APK + data folder to a logged-in client; a second command reverts. |

## How to prove a change

- Build the plugins and run the broker self-test (`sdk/pc/build_mods.cmd`), and run the Quest builder with `--dry-run`
  against your own stock APK: every step checks the stock bytes it patches.
- Run a create / join / revoke pass against a local host, on PC and on a Quest 3, and keep the logs private: host logs
  can carry codes and passwords.
- State the build in every result. A PC result and a Quest result are different claims.
