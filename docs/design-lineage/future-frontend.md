# Future Frontend

## Concept
A second, more ambitious prototype ("Future Frontend") replaces the card menu with a left tab rail
(ONLINE / PROFILE / SETTINGS / CONTROLS / HELP / QUIT), a docked Friends panel with an INVITE FRIENDS button, a
news carousel, a Help hub and a Settings screen. A related social-redesign case study critiques the shipped
wrist menu and proposes profile cards. The studio's 2022 dev log mentions a button that opens a list of
platform players to invite.

## What shipped in the game
- In the PC build and the Quest build we found no tab rail, no friends panel and no INVITE FRIENDS panel; the news
  column is static (news, season and community tiles), and the footer is a service-status and version strip.
- Pieces that are reusable do ship: the Settings screen reuses a controller-diagram texture
  (`ui_mpl_controller_diagram`) that fits the prototype's panel size; the menu type is LoneMono, shipped as
  glyph atlases (no font file), and the design frames use it.
- No public announcement that this redesign shipped was found; the archive of the studio blog is incomplete,
  so this stays unknown.

## What we built
Version 0.2.0 builds the main-menu half of this concept as **Main Menu A**, from the game's own pieces
([../../mods/main-menu-a/README.md](../../mods/main-menu-a/README.md)): a rail (ONLINE / SETTINGS / HELP / QUIT) with a
home button on every tab, a FRIENDS column, a news carousel with a season card, a HELP page with eight tiles, and a
redesigned pause menu. The rad-menu plugin fills it: the FRIENDS rows come from the host (`--friends true`), the
carousel rotates and its season slide opens the season popup, and the tab state follows the page. On PC it is a
language-package copy applied into the install and reverted by `pc-revert`; on Quest a language-package copy you push.
The PROFILE and CONTROLS tabs and the INVITE FRIENDS button were not built. Checks: [../status.md](../status.md).

## What is open
- The PROFILE and CONTROLS tabs, and an INVITE FRIENDS panel.
- A presence data source beyond the host's own friends list: the revived services have none of the platform social
  graph, and the final client's party messages are not implemented by EchoRelay.
- A news source for the column (the three slides are built in).
- Choice of art: the prototype's images are third-party and must not be copied.
