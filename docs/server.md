# Server

The client mods talk to a host running the EchoRelay fork,
[Dualgame/EchoRelay](https://github.com/Dualgame/EchoRelay) (tag `fork-2026-11`), a separate repository. It is a
derivative of the original, archived [Xenomega/EchoRelay](https://github.com/Xenomega/EchoRelay), which does not have
the invite or early-quit services these mods need. Research / educational use, LAN only; do not host public matches.

Version 0.1.0 was tested against the fork's tag `fork-2026-10` = commit `6fe21f9ed19023b303d51b02db943f0bf2c40820`;
0.2.0 (the menu update) against its tag `fork-2026-11`; see [status.md](status.md).
The menu update's host features (friends, reports, invite links, the admin page, the season card) are in `fork-2026-11`.

## Start a host

Build and run `EchoRelay.Headless` as the fork's `docs/headless.md` describes. The menu update needs:

    EchoRelay.Headless --port 777 --storage-dir ./storage --game-dir "<Echo VR install>/bin/win10" \
        --invites true --friends true --reports true --early-quit true --control-port 7779 \
        [--link-base http://myhost.lan:777] [--admin-token TOKEN] [--early-quit-mode enforce]

- `--invites true`: the `/invites` routes rad-links calls (fork `docs/protocol/invites.md`).
- `--friends true`: the friends list the FRIENDS column of Main Menu A shows and joins from (fork `docs/headless.md`).
- `--reports true`: `POST /reports`, which Report Player sends to, and `GET /reports` / `GET /reports/players`. A
  report is accepted only from a logged-in client (anything else gets 403). Each stored report has the target's id and
  name, the reason, the Mute & Ghost box, the lobby and match type, the entry point (`card`, `pause` or `tablet`
  from rad-menu; absent when the opener is unknown) and the platform (`pc` or `quest`). The host keeps what it is sent: keep its storage and logs private.
- `--control-port 7779`: the loopback control API (and the admin page below).
- `--link-base URL` and `--admin-token TOKEN`: see below. Both are optional; a token shorter than 16 characters is
  refused (the host exits with status 2).
- `--early-quit true`: the early-quit messages. The default mode, `shadow`, only records quits; with
  `--early-quit-mode enforce` the host sends lockouts and the lobby notice shown in the install pages (fork
  `docs/protocol/early-quit.md`).
- Game servers: the stock executable with the fork's patch DLL and game-server plugin beside it, built as
  the fork's `docs/headless.md`, "Build", describes (it ships no prebuilt binaries). A private match created by code occupies one game server, so run one more than the matches
  you expect, or players arriving in the social lobby get `ServerFindFailed`.
- Clients: give each player the service config (`config.json`) described in the fork's `docs/headless.md`, "Client
  config" (the six service hosts and `publisher_lock`).

## Invite links

The INVITE LINK page in the client shows `<link_base>/deeplinks/launch?code=CODE`. The host returns that link with
every invite; by default `<link_base>` is `http://<the host's LAN IPv4>:<services port>`. The host takes the first
interface that is up with an address in an RFC 1918 private range, real adapters before virtual ones (names or
descriptions containing vEthernet, WSL, Hyper-V, Docker, VirtualBox, VMware, TAP, TUN, WireGuard, Tailscale or
ZeroTier come last), and logs every candidate and its pick at start-up. If headsets cannot reach the printed
address, or to show another one (a name, a different interface), start the host with `--link-base http://myhost.lan:777`. A client can also
override it with `invite_link_base` in its own `config.json` (install pages). Opening the link in a browser on the
LAN shows the host's page for that code: live, expired or closed (read-only; it does not start the game; the page
answers 200 for a live invite, 410 for an expired or closed one and 404 for an unknown code). The client text
precedence is: the client's `invite_link_base`, then the `link_url` the host sent, then a link built from the client's
`apiservice_host`. `--link-base` must be an absolute `http://` or `https://` address without a query. Use an address
every client can reach; a headset on the LAN needs the host's LAN address, not `127.0.0.1`. Keep the address short
(about 35 characters at most): the INVITE LINK page shows the whole link on one line, and only about 64 characters
fit. A longer link is cut at both ends, and a link over 127 characters is not used at all.

## The admin page

`/admin` lists the invites (with their links) and the stored reports, and filters the reports by player id (the
xplatformid, an exact id or a prefix) and by reason (`/admin?xpid=...&reason=cheating`). The same data as JSON is at
`/admin/data` with the same two filters, and `GET /reports` takes them too.

- On the control port (`--control-port`, loopback only): `http://127.0.0.1:7779/admin`, no token.
- On the services port: only when the host is started with `--admin-token TOKEN` (16 characters or more). Without the
  token the route does not exist (404). In a browser open `http://<host>:777/admin?token=TOKEN`: the page sends the
  token on to its data call as a Bearer header, and the token stays in your browser history, so use it on a LAN you
  trust. Scripts and curl send `Authorization: Bearer TOKEN` instead. A wrong or missing token gets 401. The page is
  rate limited per address. The port is plain HTTP.

Reporter ids and invite creators are shown by their last four characters; the reported player's id is shown in full.
The page needs no game running. Treat the token and the page as private to the host's operator.

## More in the fork

`docs/headless.md` (flags, control API, build), `docs/protocol/` (invites, early quit, control API, lobby settings),
`docs/regions.md`, `docs/LICENSES.md`, and the server-density notes in its `CONTINUING.md`. The fork follows its
upstream and has no license file.
