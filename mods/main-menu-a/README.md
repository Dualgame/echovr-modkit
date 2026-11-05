# Main Menu A

A redesigned Echo VR main menu: an ONLINE / SETTINGS / HELP / QUIT rail, PLAY / CREATE GAME / JOIN VIA CODE cards,
a FRIENDS column, a news carousel, a HELP page (store, season pass, report, legal, credits, safe zone, early quit),
a home button on every tab, and a redesigned pause menu: a grid of mute / ghost / report tiles with CREATE GAME
and JOIN VIA CODE cards, a five-tile social controls card, and the Report Player pop-ups (player list, reasons, done).

Menu design by DC Coffey (Echo VR at Ready At Dawn; case study https://dccoffey.com/work/echo-vr-02). The screens
shipped inside the game but were never connected to anything; this builds the connected version.

## What ships, and what you bring

This folder holds **no game files**: only the edit scripts, the tools that apply them, and sha256 lists. The build
reads every resource it edits out of **your own** Echo VR install and checks the inputs (and, for the PC build, every
output file) against pins, so it only runs against the final PC release (build `6323983201049540`) and, for the
optional Quest package, the final Quest release (`6528386917200980`, version code 4987566).

## What it changes

Data only, in a new loose resource tree or in a copy of the language package: the main menu level (`mnu_master`),
the in-game pause menu level (`mnu_master_mp_ingame`), and the canvases, scripts, bindings and sprites they use. It
adds new pages and tiles and re-routes some stock buttons. It does not touch the game executable or its libraries.
The live parts (friends rows, carousel, Report Player, tab state) are code and come from the `rad-menu` and
`rad-links` plugins, built from their own folders; without them the new menu is a static layout.

## Requirements

- Python 3.10+, `pip install zstandard pillow`, and `bash` (Linux, WSL or Git Bash). The repository's
  `requirements.txt` lists both.
- Echo VR closed; a stock language package (no other mod applied to it: if you used `mods/menu-cards`, revert it
  first, Main Menu A replaces those cards).
- The PC build needs your install; the Quest build also needs the headset's whole stock data folder (`manifests/` and `packages/`) copied off it (below).

## Step 0: check your install (seconds, writes nothing)

    python3 build_main_menu_a.py check --game "<your Echo VR install dir>"

It checks the sha256 pins of the base and language manifests, says how many files the build will write, and refuses
with the reason when the install is another version or already modified. `--quest-store QDIR` checks the Quest copy
the same way.

## PC

    python3 build_main_menu_a.py pc --game "<your Echo VR install dir>" --out <new dir>

`--game` is the folder with `bin\` and `_data\` (the `_data/5932408047/rad15/win10` folder itself also works). `--out`
must not exist and must be outside the install. It takes a few minutes. The result is a loose resource tree
(`primary/<type>/v<version>/<file>`, 58 files) and the run ends with `verify: 58/58 files match expected_pc.tsv`. The
build only reads your install; nothing in it is changed.

### PC install: package the tree into your language package

The game reads its language package (`2b47aab238f60515`) after the base package at start-up, and a later row wins. So
the tree goes in the same way as on Quest: one row per tree file appended to a copy of your own language package. No
code runs in the game for this step, and no plugin or loader is needed to see the menu.

1. Package (seconds; reads your install, writes only into `<pkg dir>`, which must not exist):

       python build_main_menu_a.py pc-package --game "<your Echo VR install dir>" --pc-tree <PC tree> --out <pkg dir>

   It checks your stock language pair and the tree against their sha256 pins, appends the 58 rows, reads every new
   row back, and ends with `verify: stock rows unchanged, 58/58 new rows read back`. `<pkg dir>/menu` holds the new
   pair, `<pkg dir>/stock` a copy of your stock pair. `--dry-run` does all of it in memory and writes nothing.

2. Close Echo VR, then apply:

       python build_main_menu_a.py pc-apply --game "<your Echo VR install dir>" --out <pkg dir>

   It refuses if the install's language package is neither stock nor this Main Menu A pair (for example
   `mods/menu-cards` is applied: revert that first). It ends with `install language package: stock -> menu`.

3. Start Echo VR as usual: the main menu is Main Menu A. Add the `rad-menu` and `rad-links` plugins (their folders)
   for the live parts.

`pc-apply` and `pc-revert` write into the install. Under `C:\Program Files` that needs a Command Prompt or PowerShell
started with **Run as administrator**, with Windows Python (these two steps and `pc-package` need only Python and
`zstandard`, not bash). `check` tells you the state at any time: `PC language package: stock`, `... Main Menu A is
applied`, or `... modified by something else`.

**Keep `<pkg dir>`.** It holds the stock copy that revert puts back.

### PC revert

Close Echo VR, then:

    python build_main_menu_a.py pc-revert --game "<your Echo VR install dir>" --out <pkg dir>

It checks `<pkg dir>/stock` against the stock pins before writing and ends with `install language package: menu ->
stock`. If you lost `<pkg dir>`, repair or reinstall the game from your launcher: it restores the stock language
package. Rebuilding (`pc`, `pc-package`) needs the stock pair, so revert before you rebuild.

## Quest

1. Install the game on the headset as in the install page, so that `/sdcard/readyatdawn/_data/5932408047/rad15/android`
   exists. `QDIR` is your stock data folder: the folder holding `manifests/` and `packages/` that you pushed to the
   headset (the build reads the base package `48037dc70b0ecab2` and the language pair `2b47aab238f60515`; the Quest
   copies of the social card and the report layer need the whole base package, so three single files are not
   enough). If you no longer have that folder, pull it whole, before any language change:

       adb pull /sdcard/readyatdawn/_data/5932408047/rad15/android/manifests QDIR/
       adb pull /sdcard/readyatdawn/_data/5932408047/rad15/android/packages QDIR/

   Keep this stock pair: it is your revert.

2. Build:

       python3 build_main_menu_a.py quest --pc-tree <PC tree> --game "<PC install>" --quest-store QDIR --out <new dir>

   `<out>/quest_tree` is the tree moved to the Quest's `_lowspec` levels and Android type dirs. `<out>/menu` holds
   `manifests/2b47aab238f60515` and `packages/2b47aab238f60515_0`: a copy of the language package with one row per
   tree file appended. The run ends with `verify: Quest language pair matches the expected build`.

3. Close Echo VR on the headset, then push the pair over the headset's own:

       adb push <out>/menu/manifests/2b47aab238f60515 /sdcard/readyatdawn/_data/5932408047/rad15/android/manifests/
       adb push <out>/menu/packages/2b47aab238f60515_0 /sdcard/readyatdawn/_data/5932408047/rad15/android/packages/

   Check with `adb shell sha256sum` against the files you pushed.

4. Build the APK with the `rad-menu` and `rad-links` plugins (`sdk/quest/quest_apk_build_release.py`), so the menu
   comes alive.

**Revert on Quest:** push the stock pair from step 1 back to the same two paths. Uninstalling the APK does not
remove the menu (it lives in the data folder), so revert first.

## Limits

- English strings only. What was checked, on which build: [docs/status.md](../../docs/status.md).
- Without the plugins, rows and counters show their build-time texts.
- On Quest the HELP tile presses were checked by event only.
- The PC route needs write access to the install (administrator under `C:\Program Files`) for `pc-apply` and `pc-revert`;
  `pc-package` and the builds write only into the folders you name.
- A Quest combat match does not start: the Quest build ships no combat level.

## License

MIT (see the repository LICENSE).

## About the `lib/menu_ui/w12a ... w24b` folders

These are build-stage folders. The builder applies them in order (oldest to newest); you do not edit them, and
renaming one breaks the builder. `--stage latest` is what this release uses.
