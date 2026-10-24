# Design lineage

**Credit.** The frontend these mods bring to life is DC Coffey's UI/UX work for Echo VR at Ready At Dawn, described in
their case study at https://dccoffey.com/work/echo-vr-02 (a redesigned main menu, the Create & Join invite flow,
report and early-quit dialogs, and a "Future Frontend" with a tab rail and friends panel). Much of that work already
shipped inside the game's final builds, fully drawn but never connected to a service. This project did not design
it: we hooked up the functions behind it so the community can use it. Read the case study for the design itself;
this repository links to it and never copies or embeds its images.

Each page compares one of those concepts with what the game actually ships and what this project built.
Every page has four sections: **Concept**, **What shipped in the game**, **What we built**, **What is open**.

Evidence is stated per build. "PC build" means PC build `6323983201049540` (2023-05-03); "Quest build" means
Quest build `6528386917200980` (version code 4987566, 2023-04-24). Both are the final real releases on
their platform. Statements about absence mean "not found in that build's shipped resources and executable".

| page | concept |
|---|---|
| [create-and-join.md](create-and-join.md) | Create game / join via code invites |
| [main-menu-cards.md](main-menu-cards.md) | Main menu cards |
| [future-frontend.md](future-frontend.md) | Tab rail, friends panel, news |
| [report-player.md](report-player.md) | Reporting a player |
| [early-quit-loyalty.md](early-quit-loyalty.md) | Early quit, lockout, loyalty |
| [report-issue.md](report-issue.md) | Report Issue in Help |

This is a research / educational project for LAN use. It does not host public matches.
