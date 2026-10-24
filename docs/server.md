# Server

The client mods talk to a host running the EchoRelay fork,
[Dualgame/EchoRelay](https://github.com/Dualgame/EchoRelay) (branch `main`), a separate repository. It is a
derivative of the original, archived [Xenomega/EchoRelay](https://github.com/Xenomega/EchoRelay), which does not have
the invite or early-quit services these mods need. Research / educational use, LAN only; do not host public matches.

This release was tested against the fork's tag `fork-2026-10` = commit `6fe21f9ed19023b303d51b02db943f0bf2c40820` ([status.md](status.md)).

## Start a host

Build and run `EchoRelay.Headless` as the fork's `docs/headless.md` describes. The two features need:

    EchoRelay.Headless --port 777 --storage-dir ./storage --game-dir "<Echo VR install>/bin/win10" \
        --invites true --early-quit true --early-quit-mode enforce

- `--invites true`: the `/invites` routes rad-links calls (fork `docs/protocol/invites.md`).
- `--early-quit true`: the early-quit messages. The default mode, `shadow`, only records quits; with
  `--early-quit-mode enforce` the host sends lockouts and the lobby notice shown in the install pages (fork
  `docs/protocol/early-quit.md`).
- Game servers: the stock executable with the fork's patch DLL and game-server plugin beside it, built as
  the fork's `docs/headless.md`, "Build", describes (it ships no prebuilt binaries). A private match created by code occupies one game server, so run one more than the matches
  you expect, or players arriving in the social lobby get `ServerFindFailed`.
- Clients: give each player the service config (`config.json`) described in the fork's `docs/headless.md`, "Client
  config" (the six service hosts and `publisher_lock`).

## More in the fork

`docs/headless.md` (flags, control API, build), `docs/protocol/` (invites, early quit, control API, lobby settings),
`docs/regions.md`, `docs/LICENSES.md`, and the server-density notes in its `CONTINUING.md`. The fork follows its
upstream and has no license file.
