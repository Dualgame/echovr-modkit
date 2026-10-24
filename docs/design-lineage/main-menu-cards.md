# Main Menu Cards

## Concept
The redesigned main menu shows four tall portrait art cards: PLAY (with a CHANGE SOCIAL LOBBY sub-button),
CREATE GAME, JOIN VIA CODE and TUTORIAL. A footer row shows MY INVITE CODE (masked, with an eye toggle) and an
INVITE LINK button. A first-run state dims every card except PLAY; Echo Combat cards can be locked
(not purchased, or requires Link / Air Link). Left column: Settings, Credits, Legal, Quit. A news column sits on the right.
The studio's 2022-06-16 patch notes say only "Updated the Main Menu."

## What shipped in the game
- In-game reference pictures of the 2023 main menu show two cards: PLAY and TUTORIALS (plural, as shipped).
- The card canvas `ui_mnu_card` (640x904) is shared by the whole flow; its art atlas
  (`ui_menu_frontend_button_tile_01`, `0x3c8a8ab44fd6ee99`) names regions including `tile_ec_map_dyson`,
  `tile_ec_control_point`, `tile_ec_payload`, `tile_echo_arena` and `tile_echo_combat`.
- Lock reasons "Requires Combat Purchase" and "Requires Oculus Link or Air Link" exist in the create page script.
- The footer shows INVITE CODE: (not MY INVITE CODE). The shipped news tiles differ from the design's.
- The same sizes match the design at one scale: the shipped card canvas scaled to the design's card width
  reproduces the case study's card, bar and column positions to within a pixel or two, i.e. its frames were built on the
  shipped canvases (or the reverse).
- CREATE and JOIN card owners were not found on any menu page in the PC build; no card for them is wired.

## What we built
- **PC:** the cards come from patching the menu's page data when the game loads it, so the stock files on disk are
  not touched. The patched pages add CREATE GAME and JOIN VIA CODE cards that open the shipped wizard.
- **Quest:** a language-package override adds the card entries and leaves the stock files alone. It needs no change
  to the game library. The override is written for en_us only.
- Both were run on PC build `6323983201049540` and Quest build `6528386917200980` (Quest 3), together with the
  invite flow in [create-and-join.md](create-and-join.md). The cards reuse the shipped card canvas; no new art was made.

## What is open
- The MY INVITE CODE footer (the shipped footer reads INVITE CODE; it is not wired to a created invite).
- Card art for CREATE GAME and JOIN VIA CODE.
- The cards in other languages (CONTINUING: other languages).
- Whether these cards ever appeared in a live build.
See [../../CONTINUING.md](../../CONTINUING.md).
