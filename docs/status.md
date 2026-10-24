# Status

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
