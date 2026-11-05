# Report Player

## Concept
The design proposes reporting from several entry points (an in-game grid or dropdown, the pause menu, and
variants of an arm-computer button), a player list, then a confirmation "<player> will be reported for violating
the Echo VR Code of Conduct" with six reasons (hate speech, cheating, harassment or bullying, inappropriate
behavior, unsportsmanlike conduct, impersonation of staff) or a nine-reason grid, a "Mute & Ghost Reported
Player" check, and CANCEL / SUBMIT. The popup reuses the early-quit tablet frame.

## What shipped in the game
- Nothing of the flow ships. In the PC build, no parsed canvas or string contains it (275 of 277 canvases
  parse), and the executable has no "Report Player" text. `ui_mpl_poster_report` is a known name but is in no
  checked build (PC final, last `_lowspec` PC, Quest).
- Nearest pieces: the arm-computer SOCIAL CONTROLS card (MUTE / GHOST / INVITE TO PARTY / ADD FRIEND, no
  REPORT) and one string, "report harassment or other toxic behavior". The card ships and renders to the right of
  the tablet on PC and Quest when its script function is called, but no shipped button opens it.
- The tablet atlas has a region named `report_icon`, but it is Lone Echo's dialogue speech bubble (three dots), not
  a report-player icon, and nothing in Echo VR uses it. Whether it was kept for a report feature is unknown. It is a
  ready-made glyph for one either way.
- The studio said in 2022 that in-game reporting was ongoing; no later release note was found.

## What we built
Version 0.2.0 of the kit (checks: [../status.md](../status.md)):

- **Entry points.** The pause menu has a REPORT tile that opens three popup pages (player list, six reasons with a
  Mute & Ghost box, SUBMITTED). The SOCIAL CONTROLS card gets a fifth tile, REPORT. On the tablet, the stock LOBBY
  list (native scroll, no "more players" rows) opens that card; its REPORT opens the design's REPORT PLAYER panel on the
  tablet (3 × 2 reasons grid, Mute & Ghost option, CANCEL / SUBMIT; the pause menu's reasons page is the automatic fallback). As in the design, the panel's top-right corner shows "<" (back one step) where the list and card show X (close the tablet); "<" and CANCEL return to the list with the player's card open, the design's previous screen. The pause list scrolls (five rows and a peek row). An earlier draft drew its own tablet player list with paged
  rows, a tablet reason grid or dropdown and a large player card; those were removed in favour of the game's own
  scrolling list.
- **Where it goes.** One `POST /reports` to your own host (the fork with `--reports true`): the reported player's id and
  name, the reason, the Mute & Ghost box, the lobby and match type, and the entry point and platform. The host's
  `/admin` page lists them and filters by player id and reason. AI bots have no account and are left out of the lists.
- **ADD FRIEND** on the five-tile card is an opt-in on PC (`card_add_friend=1`, off by default: the game's card scripts switch the tile off again); Quest's stock script already shows it; the press goes to
  the platform's friends service, not to the host.
- Checked on PC `6323983201049540` and Quest `6528386917200980` by event-driven presses: the pause path in both
  directions (PC to Quest player, Quest to PC player), the tablet grid and the large card on PC.

## What is open
- The Mute & Ghost box is only recorded in the report; applying it through the social card's own MUTE and GHOST is not
  built.
- The design's nine-reason grid and 3 × 2 reason tiles: the large card holds four tiles at most, so the six reasons
  stay on the pause popup and the tablet.
- The large player card on Quest, the dropdown and the five-tile card's REPORT tile are not yet tested; the card's
  header (the player's name) is blank.
- What a research LAN does with reports: they are stored and listed, not acted on.
