# Report Issue

## Concept
The case study says a Report Issue entry was "immediately added to the Help tab".

## What shipped in the game
- In the PC build, the Help popup's script (`0x12ae726b0773d988`) contains four hyperlink literals:
  `https://link.medium.com/AjO6eH1H8sb`, `https://medium.com/echo-games-blog`, `https://bit.ly/3y79nDp` and
  `https://readyatdawn.zendesk.com/hc/en-us` (a support site). Static analysis shows an expression copying
  these into buffers; it did not connect them to the popup's initially empty help and Discord link buffers.
- Whether any button opens the support URL at runtime is unconfirmed, and these are shipped strings, not verified
  live destinations. Do not assume which short link is Discord.
- The popup's group-link and hyperlink nodes are registered and consumed, but runtime behavior has not been observed.

## What we built
Nothing for this concept.

## What is open
- Trace which control opens the support link, and whether it is reachable (CONTINUING: Report Issue).
- A replacement destination for a revived community (the original support site is the studio's).
