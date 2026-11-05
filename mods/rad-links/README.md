# rad-links — invite codes for Echo VR PC's shipped CREATE GAME / JOIN VIA CODE wizard

A clean-room client plugin for the final Echo VR PC build (`echovr.exe`, `6323983201049540`, 2023-05-03). The
wizard's pages (`mnu_master` 34–41) shipped as a client-side stub. rad-links connects them to an
EchoRelay host's `/invites` routes and joins the resulting session in process. No page script is edited.

| player does | rad-links |
|---|---|
| p36–p39 cards (experience, mode, map) | remembers them (`evt_card_clicked`) |
| p40 CREATE INVITE CODE & LINK | from the page's own `create_invite` metric (settings + player id): `POST /invites` → code + link |
| p41 JOIN | joins the invite it just created |
| p34 types a code + ENTER | from the page's own `join_via_code` / `join` metric (`join_code`): `GET /invites/{code}` → joins its link |
| p34 resolve fails | a message in the field's title bar (`title_text` of actor `0xad97e58a7b13425f`): the host's `reason` first, else the status: `INVALID CODE` (404 / `invalid`), `CODE EXPIRED` / `CODE CLOSED` (410), `ENTER PASSWORD` / `WRONG PASSWORD` (403), `INVITES OFF` (503 `invites_off`), `NO SERVER FREE` (503), `TRY AGAIN LATER` (429), `NO CONNECTION`, `SERVER ERROR` |
| p34, password invite (403) | title `ENTER PASSWORD`; the next ENTER within 2 min is the password (with or without the code still in front of it): `GET /invites/{code}?password=…`; a wrong one reads `WRONG PASSWORD` and asks again; CANCEL / BACK drops the prompt |

**Join**: at the main menu (NetGame state 3) it stages CR15Game's `-lobbyid` block and runs "Beginning
multiplayer", the function PLAY runs, so the netgame joins the link instead of matchmaking. Like PLAY, it shows
the STATUS page (`mnu_master` 51) first and begins a second later, so the player sees the connection. In a
lobby or match it calls `CR15NetClientLobby::Join`.

**Texts on both platforms:** after a create, p41 shows the code and `EXPIRES: HH:MM UTC`, and p35 (the INVITE
LINK card) shows the invite's web link (`CCanvasUICS::SetText` on each actor's `canvasui`; the code script's own
`UpdateCode` refuses outside calls). See "Invite links" below.

## Invite links

The INVITE LINK card (opened by the home INVITE LINK button, and by the create wizard's share page) shows one line,
`<link_base>/deeplinks/launch?code=CODE`, where CODE is the invite code. Joining is unchanged: the `-lobbyid`
boot argument stays the internal join mechanism, and only the text on the card differs.

Which base the card shows, first match wins:

1. `invite_link_base` in the client's config (the same `config.json` that holds `apiservice_host`; PC
   `_local\config.json`, Quest `/sdcard/readyatdawn/_local/config.json`), for example
   `"invite_link_base": "http://myhost.lan:777"`. It must start with `http://` or `https://`; anything else is ignored
   and logged.
2. The `link_url` the host returns with each invite (the EchoRelay fork builds it from `--link-base`; its default is
   the host's first LAN address and the services port). Used only when it holds the invite's code; a link for another code is skipped.
3. `http://HOST[:PORT]` from `apiservice_host`, plus the path.

If none of these gives a usable address (no code, or a link over 127 characters) the card keeps the older text,
`-lobbyid <id>`; the note under the link ("open the link to see the code, or enter it in JOIN VIA CODE") is written only while the card shows one of our web links, not beside that fallback. Opening the link in a browser on the same network shows the host's page for that invite (live,
expired or closed); it does not start the game.

**p34 messages and the password prompt:** the p34 error messages run on PC and Quest. A password join works from a PC
creator to a Quest joiner, but the p34 field holds only 5 characters, so the typed code has to be cleared first;
making that one flow is open.

Not yet: no protocol handler or installer; the in-match wizard (`mnu_master_mp_ingame` 8–14) is not mapped: its page
owners differ from p36–p41, so a create there is not seen. Invites for AI TEAMMATES games used to crash the dedicated
server; that is fixed on the host side in the EchoRelay fork (it hands team-Any joiners the creator's team).

## Exports for other plugins (Q3 "See this friend", rad-menu's FRIENDS rows)

Resolve with `GetProcAddress` on the loaded rad-links module. Both only queue work (any thread); the join runs on the
next ticks through the wizard's own `join_link` (main menu: STATUS page, stage `-lobbyid`, begin multiplayer; lobby or
game: leave / cancel, then `Join`). Return 0 = queued, -1 = bad argument, -2 = rad-links not running.

- `int radlinks_follow(const char *guid, const char *gametype)`: join a session id or invite link by GUID; `gametype`
  is a name, `0x<16 hex>` (the symbol itself, as the host's `/friends` row gives it), or NULL / "" (-1).
- `int radlinks_join_code(const char *code)`: a GUID → `radlinks_follow(code, NULL)`; otherwise an invite code,
  resolved through `GET /invites/{code}` exactly as JOIN VIA CODE.

Log: `follow 8f4e1d2a..(36) (gametype …): queued` / `join code D..(5): queued (resolve, then join)`, then the usual
`join …` lines (codes, links and session ids are masked in every log line; see Log below).
The host accepts a raw SessionId as the join target (`GameServerRegistry.GetGameServer` looks up session ids before
aliases; `source-only`). Built 2026-10-30, no game run yet.

## Build

```bat
build.cmd        rem MSVC x64 (BuildTools 2022): build\rad-links.dll
```

`build.cmd` finds `evrmod.h` and `evrmod_profile_pc.h` through `EVRMOD_INC` (see the script for its default). It writes
no debug info unless `DEBUG=1` is set.

This builds the same DLL as the SDK's `sdk\pc\build_mods.cmd`, which is the route the install page uses
(`sdk\pc\build\mods\rad-links.dll`); either copy works.

## Run

Put `rad-links.dll` in the `mods\` folder beside the evrmod `winmm.dll` loader (`sdk/pc`), which gives it the broker's
hook sites; or inject it into `echovr.exe` yourself, in which case it hooks the same sites itself and chains onto a
hook another tool already put there.

- Services: `apiservice_host` in the install's `_local\config.json` (the address the client logs in to). Optional
  `invite_link_base` in the same file sets the base of the INVITE LINK card's link (see "Invite links").
- Log: `_local\radlinks.log` beside the exe. Invite codes, links and session ids never appear whole (first quarter
  and length: `D..(5)`, `8f4e1d2a..(36)`; your own code and link as lengths only), and no player id is written, so
  the log can be shared. The Quest build logs the same way.
- The EchoRelay host must serve the `/invites` routes (start it with `--invites true`).

## Quest (`libradlinks.so`)

The same plugin for Echo VR Quest `6528386917200980` (vc 4987566): `src/radlinks_quest.c`, built by `build_quest.sh`
(NDK r26d) into `build/libradlinks.so`. It shares no code with the PC file. The menu pages are PC's bytes on the
Quest, so the wizard logic is the same. No instruction is patched: only RELRO words are swapped after a
prologue check of the 15 functions it uses:

| hook | word | why |
|---|---|---|
| metric | GOT `0x36da2c8` → `NR15Logger::MetricLog(char const*, u64)` `0x120cc50` | settings and the typed code (only caller `CR15NetJsonLog::WriteLog`, through the PLT) |
| target | GOT `0x36dbf40` → `SendComponentEvent` `0x1807264` | card choices, p41 JOIN, p34 ENTER |
| tick | `CR15Game` vtable slot `0x362ee80` | the join runs on the game thread |

- **Loading.** The `libevrmod.so` mod loader (`sdk/quest`) adds itself to `libr15.so`'s `DT_NEEDED` and `dlopen`s
  every `libevrmod_*.so` from its constructor; the APK builder installs this library as `libevrmod_radlinks.so`.
  Standalone, the library also works from an `LD_PRELOAD` entry.
- **Player id.** The JSON `user_id` is broken on the Quest. The id is built from
  `CR15NetGame::XPlatformId()` `0x1244628` and the prefix table (`OVR-ORG-<n>`).
- **Join.** At the main menu it stages `CR15Game`'s `-lobbyid` block (GUID `+0x8538`, gametype `+0x8560`; on the
  Quest `+0x8568` is the region) and runs `CR15Game::BeginMultiplayer()` `0x1201be4`, what PLAY runs. In a lobby or
  match it calls `CR15NetClientLobby::Join` `0x12462a8`. Offsets are in the source; all were found by disassembly of
  that one build.
- **Services.** The engine's own `CJson::TString(&netgame->config, "apiservice_host", <API literal>)`, asked on the
  game thread at login: the loaded config (the `/sdcard` file, tier 2 or the APK's) wins, else the `libr15.so` API
  literal. `HOST` may be a name (`getaddrinfo`); an https, unparseable or unresolvable host is logged
  (`services: UNUSABLE …` / `cannot resolve …`) instead of a silent `127.0.0.1:7777`. Plain HTTP/1.0. The
  `quest_libr15_api_config_patch.py` step of the APK builder gives the engine's own API connections the same rule.
- **Per word (no module is looked up by name):** a stock word is swapped; a word that already points into an
  executable mapping of another loaded module (`/proc/self/maps` + `dladdr`, compared by load base, not libr15.so,
  not rad-links) is chained; anything else is refused with a log line saying why, and rad-links stays inert. When
  another module from the APK's lib directory is mapped before it, rad-links lets the three words settle (quiet 1 s,
  max 3 s) before swapping, so that module swaps first and rad-links chains onto it (both threads see the same
  relocation event; without the wait the order is a race). A tool loaded after rad-links that refuses non-stock
  words will refuse the words rad-links holds, so load such a tool first.
- **Log:** `adb logcat -s radlinks:V` (with `evrmod:V` for the loader and `RAD:I` for the game). It also writes
  `files/radlinks.log` in the app's private storage, which a release (non-debuggable) APK cannot read back, so use
  logcat. The metric and wizard events arrive on a script worker thread, not
  the tick's; the hooks only copy and queue, and the join runs on the tick.
- **Main-menu cards on the Quest:** staged through the language package, with no APK change (`mods/menu-cards`).

## Safety

- It refuses any image other than `6323983201049540` (Quest: `libr15.so` of `6528386917200980`, 15 prologues), and any hook site, Join, LeaveGame, cancel or
  begin-multiplayer prologue whose bytes differ from the profile.
- Hooks copy and queue only. HTTP runs on a worker thread; the join runs from the game tick.
- It never touches the client's local HTTP API (127.0.0.1:6721).
