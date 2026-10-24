# Architecture

How the pieces fit. This is a research / educational project for LAN use; it does not host public matches.
Evidence is stated per build: PC `6323983201049540` and Quest `6528386917200980` (version code 4987566). Byte
layouts and addresses here belong to those builds only.

## 1. The invite flow

The final builds ship a CREATE GAME / JOIN VIA CODE wizard that no service ever backed, and the game's services
protocol has no link or code message. So the contract is ours and it rides on plain HTTP to the services port, with
one proven join at the end of it.

```
 player presses wizard cards / CREATE INVITE CODE / types a code      (stock page scripts, unchanged)
        |
        v
 rad-links   (client plugin in the game process; PC dll, Quest .so)
        |  observes script events and the page's own click metric
        |  HTTP on a worker thread -> services port
        v
 EchoRelay host  /invites  (fork, Invites.cs, via the Server.ExtraApiHandler hook)
        |  create : lease an idle game server, start a private session, add a link alias, mint a 5-char code
        |  resolve: code (+ password) -> link, game type, level
        v
 ONE join path:  link -> lobby join request -> session success -> UDP to the game server
        ^
        |  the creator joins the same way as everyone else
```

**Create.** When the player presses CREATE INVITE CODE & LINK the stock page logs its settings (experience, mode, map,
password, region, expiry) and the player id as a click metric. The metric is sent to the services as a remote-log
message; rad-links reads it inside the game process instead of adding a new network message. It POSTs `/invites`
with those values. The reply carries the code, the link, the expiry and whether a password is set. The plugin then
writes the code, `EXPIRES: HH:MM UTC` and the `-lobbyid <link>` text into the Share pages.

**Join.** Typing a code on the JOIN VIA CODE page logs a join metric with the code. rad-links resolves it with
`GET /invites/{code}`. The host answers with the link, or an error that the plugin shows in the field title (404 invalid,
410 expired or closed, 403 password required or wrong). The plugin then joins the link in process. From the main menu it stages
the game's `-lobbyid` boot block and runs the routine PLAY runs, so the netgame joins the link instead of matchmaking
(it shows the status page first, as PLAY does). In a lobby or match it calls the client's join routine (a typed-code join from a social lobby is not yet tested; see CONTINUING).

**Password.** A 403 from `GET /invites/{code}` means a password is set. The plugin sets the field title to ENTER
PASSWORD and takes the next ENTER within two minutes as the password, then repeats the request with `?password=`. A
wrong one reads WRONG PASSWORD and asks again; CANCEL or BACK drops the prompt. The host keeps only a hash. Over plain
HTTP the password still travels in clear, and the game's own create metric carries it too, so keep host logs private.

**Region.** The wizard's region label (ten of them) is mapped to the shipped datacenter code (for example US WEST to
`usw`) by the host and sent in the create. A game server registers the region it was started with (its config key
`server_region` or the `-serverregion` option, else `default`). Matching prefers exact region matches, then `default`
servers, then any. The fork also fixes a read of the registration message that used to put the region into the
version-lock field.

**Where the host can be reached.** The game's own HTTP API listens on loopback only, so nothing on the network can call
into a client through it. Every request therefore starts at the client: client to host is HTTP from rad-links. The
host cannot push to a client, so any later "your invite was joined" style feature must be polling or a channel the
plugin opens itself.

## 2. The early-quit wire

The final clients contain the client half of an early-quit system (notice, two INFO pages, lockout timer, menu card,
podium pages, scoreboard ribbons). The service half was never public. The fork implements it
(`--early-quit true`).

**Host to client, on the login connection only.** The client never sends early-quit messages.
1. *Config*: a one-byte struct, a 32-bit length, then a zstd-compressed JSON of two tables (penalty levels and
   steady-player levels). The leading byte is load-bearing: without it the client reads the length one byte late and
   dies in its stack allocator.
2. *Feature flags*: one byte. Bit 5 gates the EARLY QUIT card, the quit notice, the podium pages, ribbons and the
   post-match display; bit 1 gates the lockout timer.
3. *Update notification* (56 bytes): quit count, completed and quit match counts, penalty level, loyalty tier index,
   a penalty timestamp that must increase, and a lockout expiry.

**Client unmask.** The stock client stores the flag byte as `byte & 0xdd`, which clears bits 1 and 5 for good, drops
the Update's expiry (storing -1), and fixes its "lockout active" output to 0. So a stock client can show only INFO 2/2.
The client patch keeps every flag bit, keeps the expiry, and computes active as "expiry set and in the future". PC: four
sites, 23 bytes, patched in memory by the `evrmod_earlyquit.dll` plugin, so the executable on disk stays stock
(`mods/early-quit/tools/pc_exe_earlyquit_patch.py` writes the same sites into a copy of the executable, as a
reference and check tool only). Quest: six sites, 24 bytes, in the builder's copy of the game library. Both are
fixed-width in-place edits with no code cave, and both can be reverted to the stock bytes.

**The lockout itself is the host's.** The client has no lockout logic; it never writes an expiry. A lockout is the
services refusing matchmaking: with `--early-quit-mode enforce` the host refuses public FIND / JOIN / CREATE with
"server is locked". In the default shadow mode it only records and logs. In enforce mode the login sends flags `0x02`,
and bit 5 is held only briefly while a player arrives in a social lobby with a notice owed (the "arrival pulse"),
because the stock podium shows the notice only during that load.

## 3. The mod loader and hook broker

Both platforms load plugins with no launcher, no injector and no debuggable APK. Each plugin refuses a hook word that
is not stock, so plugins that patch the same site must agree on an owner: on PC that owner is the broker.

**PC.** A `winmm.dll` proxy placed beside `echovr.exe` (`sdk/pc`). The executable imports four winmm functions
statically (`timeBeginPeriod`, `timeEndPeriod`, `timeGetDevCaps`, `timeGetTime`); the proxy wraps those and forwards
the other exports, and on load starts a worker thread that loads `mods\*.dll`: first the names in
`mods\load_order.txt` if it exists, then every other DLL alphabetically. No plugin name is special. `winmm` is not a
KnownDLL, so the copy in the game directory wins the DLL search; Defender raised no detection on the test machine.
Uninstall by deleting `winmm.dll` and `mods\`.

**The broker (PC, ABI 2, `sdk/pc/include/evrmod.h`).** The proxy owns the three hooks everyone wants, installs each
once, and dispatches to the plugins that register for them: the game tick, `SendComponentEvent` and the metric
logger. It also hands plugins the build identity (a plugin refuses any build but the known one), a CSymbol64 hash
helper and fault-guarded memory reads. rad-links and the early-quit plugin use it; each keeps a standalone fallback that
hooks for itself when no broker is present. The early-quit plugin patches its four sites in memory, so the executable
on disk stays stock.

**Quest.** The APK builder (`sdk/quest`) appends `libevrmod.so` to the game library's needed-libraries list and
re-signs the APK with a key made on your machine. The loader opens every `libevrmod_*.so` in the app's native library
directory; each Quest plugin hooks for itself (there is no Quest broker yet). Creating
`/sdcard/readyatdawn/_local/evrmod.disable` turns the loader off without reinstalling. The early-quit change on Quest
stays a patch the builder applies to the game library at APK build time.

## 4. Server notes

The dedicated server is the stock executable with the fork's patch DLL (`EchoRelay.Patch`) and game-server plugin
(`EchoRelay.GameServer`). The patch DLL sets the tick rate: `-timestep` is the server tick rate (default 120), not a
game option. The plugin also relays the lobby-settings message the game server would otherwise drop (details in the
fork's `docs/protocol/lobby-settings.md`). See [server.md](server.md), and for load per host the fork's `CONTINUING.md` ("Servers per host" and the
server-density item).
