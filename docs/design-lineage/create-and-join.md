# Create & Join

## Concept
The design proposes an invite flow launched from the main menu. CREATE GAME walks through Choose Experience
(Lobby / Echo Arena / Echo Combat), a mode and map card, then Review Settings (password, server region,
expiry as stepper rows), then CREATE INVITE CODE & LINK and a Share page showing a five-character code
(example X7F9A), an eye toggle, an invite link and an expiry countdown. JOIN VIA CODE is a single code field
with in-field errors (invalid, expired) and a password branch. A further variant (Quick / Advanced Invite
with party policy) appears in an animated mock. The studio's own 2022 dev log announced the same feature set:
five-digit codes, password, region and expiry.

## What shipped in the game
- Both builds ship the wizard: 6 `ui_mnu_deeplinking_*` canvases (generate, summary, invite_code,
  invite_expiration, view_link, text_input), 15 page scripts, a settings table, plus `ui_mnu_settings_invite_regenerate`
  and a party-invite dialog. Canvases and settings are byte-identical across the PC build, the last
  `_lowspec`-era PC build (`6068409776606885`, 2023-02-13) and the Quest build.
- The page scripts hold the full flow: CHOOSE EXPERIENCE, SELECT GAME MODE, SELECT MAP, REVIEW SETTINGS,
  SHARE INVITE, JOIN VIA CODE. Review Settings has four rows, one more than the design: AI TEAMMATE DIFFICULTY.
  Combat maps listed: Dyson, Combustion, Fission, Surge. Regions (10) and expiries (3 h, 6 h, 24 h, 1 week) are present.
- The shipped placeholder link is `https://graph.echo.games/deeplinks/launch?code=x7f9a` with the note "This
  link can only be used on PC." Its code matches the design's Share example, which strongly suggests the design
  is the shipped one.
- The wizard canvases are referenced only by the menu master canvases, so they load with the menu. No menu
  route reaching them was found, and the game's services protocol has no link/code message. Whether the
  entry points were ever live is unknown.
- Quick Invite, Advanced Invite, Party Join Policy and Max Party Players do not appear anywhere in the PC build:
  that variant is design-only.

## What we built
Two halves: a host service and a client plugin. Everything here is research code for a LAN; it was tested on PC
build `6323983201049540` and Quest build `6528386917200980`, on a Quest 3 only.

**Host (`/invites`, in the EchoRelay fork).** Start the console host with `--invites true`. It serves three routes on
the services port: create (`POST /invites`), resolve (`GET /invites/{code}`) and revoke. A create takes the player id,
game type, level and optionally region, password, expiry and player limit. The host leases one idle game server,
starts a private session on it and registers a link alias for that session, so an ordinary lobby-join request with the
link reaches it. The code is five characters without `0 O 1 I`. A wrong password gives 403, an unknown code 404, an
expired or revoked one 410. Passwords are stored hashed. Route table and limits: the fork's `docs/invites.md`.

**Client (`rad-links`).** A plugin for the final PC and Quest builds. No page script is edited. It reads the wizard's
own create metric (settings and player id) and POSTs it to `/invites`, then shows the code, the expiry time and the
`-lobbyid <link>` text on the Share pages. For JOIN VIA CODE it reads the typed code from the page's join metric,
resolves it and joins. A failed resolve is shown in the code field's title: INVALID CODE, CODE EXPIRED, CODE CLOSED,
NO CONNECTION or SERVER ERROR. A password invite turns the title into ENTER PASSWORD, and the next ENTER within two
minutes is taken as the password. The join runs in the game process: from the main menu it does what PLAY does but
with the invite's link instead of matchmaking; in a lobby or match it calls the client's join routine. The PC plugin
and the Quest library share no code. Each refuses to load on any other build and checks the bytes at every hook site.

**What works today.**

| item | PC | Quest |
|---|---|---|
| create, code and expiry on the Share page | yes | yes |
| join by code, either direction (PC creator, Quest joiner and back) | yes | yes |
| p34 error messages | yes | yes |
| AI TEAMMATES invites | yes | yes |
| password join (the player presses CLEAR at the password prompt: the field keeps the 5-character code) | yes | yes |
| region: a create lands on a server in the asked region, and the joiner's server-location readout shows it | yes | yes (as joiner) |
| Quest cards that open the wizard | n/a | yes (see Main Menu Cards) |

Not built: a protocol handler or installer that opens a link from outside the game, and the in-match wizard (its page
owners differ from the main-menu wizard's, so a create there is not seen). Hostnames are not resolved by the plugin:
it takes an IPv4 literal for the services host. How the pieces connect: [../architecture.md](../architecture.md).

## What is open
- Quick / Advanced Invite and the party join policy (design-only; nothing shipped to reuse).
- An in-lobby wizard (CONTINUING: P4 in-lobby wizard).
- Clearing the field automatically at the password prompt, a non-default region chosen in the wizard, a shareable
  link text, hostnames in the plugin.
- Persistence, immediate revoke and authentication of invite creation and use (CONTINUING: host H-1, H-2, H-4).
  Invites live in memory only and any logged-in id is accepted, so use this on a LAN only.
- Whether the shipped build's own entry points were ever reachable by players.
Each item has an entry point and an acceptance test in [../../CONTINUING.md](../../CONTINUING.md).
