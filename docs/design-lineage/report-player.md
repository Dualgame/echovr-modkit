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
- Nearest pieces: the arm-computer social controls canvas (MUTE / GHOST / INVITE TO PARTY / ADD FRIEND, no
  REPORT) and one string, "report harassment or other toxic behavior".
- The studio said in 2022 that in-game reporting was ongoing; no later release note was found.

## What we built
Nothing for this concept.

## What is open
- A report endpoint: where reports go, and who reads them on a LAN (CONTINUING: Report Player).
- The arm-computer REPORT row and the reason dialog on the shipped tablet frame.
- Whether a research LAN needs this at all.
