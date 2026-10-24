# Early Quit / Loyalty

## Concept
Quitting a match early produces a warning, repeated quits a temporary lockout, and a long-term Loyalty
Rating with tiers and ribbons. The design prototype adds a quit-confirmation popup (YES / NO), early-quit notice
and lockout warnings for the menu, in-game and party leader, and a loyalty progress meter. The studio's 2022
dev log announced early-quit detection, warning and penalties as in progress.

## What shipped in the game
Copy and UI ship in both the PC build and the Quest build (identical strings in the same canvases):
- EARLY QUIT INFO 1/2 and 2/2, a QUIT NOTICE / LOCKOUT NOTICE (variants), a red LOCKOUT countdown tab, arm
  computer "LOCKOUT / PARTY LOCKOUT", and lobby-podium pages with the reason "Matchmaking Lockout".
- Promised behavior: a notice per early quit; repeated quits trigger a temporary lockout on matchmaking and
  joining; later quits extend it and lower Loyalty Rating; rating needs 10 completed matches; tiers Steadfast,
  Noble, Loyalist, The Lifer, each with a title; ribbons on the scoreboard; slow decay.
- The typos "matchmatch" / "matchmatching" ship as written. Not shipped as text: "LOCKOUT ACTIVE", an expiry
  line, any forgiveness rule. The quit-confirmation wording was not found in canvases.
- The client masks the early-quit state flags it receives (`& 0xdd`, clearing bits 1 and 5) and gates the info page, so stock builds show
  none of it in practice. No server-side rule or durations are given anywhere in the text.

## What we built
Two halves again, run on PC build `6323983201049540` and Quest build `6528386917200980` (Quest 3).

**Host (EchoRelay fork, `--early-quit true`).** It sends the client three messages on the login connection: the
config (penalty and loyalty tables), the feature flags, and a per-player update (quit counts, penalty level, loyalty
tier, lockout expiry). Byte layouts are in the fork's `docs/protocol/early-quit.md`.
- **Shadow mode is the default.** It detects quits, records them and logs what enforce would do. It refuses
  nothing and shows no notices. **Enforce** (`--early-quit-mode enforce`) is opt-in and makes the lockout real by
  refusing public FIND / JOIN / CREATE; social, private, AI and spectating are never gated.
- Quits are counted over the last 20 public arena / combat matches (30-day cap). Warnings start at 3 quits and a
  lockout at 5; lockouts run 5 min, 15 min, 1 h, 6 h, 24 h. Every value is policy and can be replaced with a JSON file.
  These choices are ours, informed by public leaver-penalty systems, not Echo VR's.
- Match completion is inferred from the session end, because the game server does not report it.
- A built-in self-test runs the policy (`--early-quit-selftest`).

**Client patch.** The stock client keeps only some of the flags it is sent, and ignores the lockout expiry. A small
patch to a copy of the PC executable (four sites) and of the Quest game library (six sites) keeps all flags and the
expiry, so the shipped notice, lockout timer, INFO pages, menu card and podium pages display. The Quest patch is
applied when the APK is built; on PC a plugin applies the same four changes in memory, so the executable on disk
stays stock (see [../architecture.md](../architecture.md)).

**What works today.** On a patched client: the three messages (byte-exact), the podium banner, notice and timer,
two-client quit detection, the menu card, lockout expiry and the "arrival pulse" that shows the notice when a player
arrives in a social lobby; on PC and Quest. A stock, unpatched client shows only INFO 2/2.

**Known limits.** Enforce mode is **not supported** until the post-match timing is measured and tuned in a public match. A stock
client shows its fixed "SERVER IS LOCKED" text for a refused request, not the host's message.

## What is open
- Post-match timing and enforce mode (the fork's CONTINUING), plus the remaining tests on the device.
- Ribbons and titles (never seen on a device), and the progress meter's canvas (CONTINUING: ribbons and titles).
- The quit-confirmation popup, which never shipped.
- Tier labels per index, and where the lockout timer sits in game.
See [../../CONTINUING.md](../../CONTINUING.md).
