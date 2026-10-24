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
Nothing for this concept.

## What is open
- New canvases for the rail, friends panel and carousel (CONTINUING: Future Frontend canvases).
- A presence / friends data source (CONTINUING: SNSFriend* messages). The revived services have none of the
  platform social graph, and the final client's friend and party messages are not implemented by EchoRelay.
- A news source for the column.
- Choice of art: the prototype's images are third-party and must not be copied.
