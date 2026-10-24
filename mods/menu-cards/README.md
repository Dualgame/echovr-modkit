# menu-cards

Adds CREATE GAME and JOIN VIA CODE cards to the Echo VR main menu. The cards open the shipped invite wizard that
rad-links drives. They are optional: the wizard works without them.

The cards are data, not code. `menu_cards.py` copies the main menu page out of your own install, inserts the two
card elements, and appends the result as one row to a copy of the language package (`2b47aab238f60515`). The game
loads that package after the base package and the later row wins. Every stock input is checked against a sha256 pin
first, so another game version, or a language package that is already modified, is refused.

Targets: PC build `6323983201049540` and Quest build `6528386917200980` (version code 4987566). Cards exist for
en_us only. What has been tested, on which build: [docs/status.md](../../docs/status.md).

## Requirements

- Python 3.10+ with `zstandard` (`pip install -r requirements.txt` at the repository root).
- `mods/menu-cards/cards_page.py` beside `menu_cards.py`, and `sdk/tools/radpkg.py` (both are in this repository; keep
  the layout).
- Echo VR closed while you build, apply or revert.

## Which Python

The script reads and writes plain files, so it runs on Windows or under WSL. The paths must match the Python you run:

| you run | game path looks like |
|---|---|
| Windows Python (`py` or `python` in a Command Prompt or PowerShell) | `"C:/Program Files/Oculus/Software/Software/ready-at-dawn-echo-arena"` |
| WSL `python3` | `"/mnt/c/Program Files/Oculus/Software/Software/ready-at-dawn-echo-arena"` |

A `C:/…` path does not resolve in WSL, and a `/mnt/c/…` path does not resolve in Windows Python.

## PC

`--game` is the install root: the folder that holds `bin\` and `_data\` (the `_data\5932408047\rad15\win10` folder
itself also works). `--out DIR` is a work folder outside the install.

    python mods/menu-cards/menu_cards.py pc build  --game "C:/Program Files/Oculus/Software/Software/ready-at-dawn-echo-arena" --out C:/echovr-cards
    python mods/menu-cards/menu_cards.py pc apply  --game "C:/Program Files/Oculus/Software/Software/ready-at-dawn-echo-arena" --out C:/echovr-cards

- `build` reads your install and writes only into `DIR`: `DIR/cards/` (the patched language package), `DIR/stock/`
  (a copy of your stock language package) and `DIR/cards.json` (a report).
- `apply` copies `DIR/cards/` into the install. It refuses if the install's language package is neither stock nor this
  build's cards.
- `apply` and `revert` write into the install. Under `C:\Program Files` that needs a Command Prompt or PowerShell
  started with **Run as administrator**, with Windows Python. Whether a WSL `python3` may write there depends on your
  Windows permissions (not checked).
- `--loose` (with `build`) also writes the page as a single file under `DIR/loose/`, for a loose-resource loader.

**Keep `DIR`.** It holds the stock copy that `revert` puts back. To return to stock:

    python mods/menu-cards/menu_cards.py pc revert --game "C:/Program Files/Oculus/Software/Software/ready-at-dawn-echo-arena" --out C:/echovr-cards

`revert` checks the stock copy in `DIR/stock/` against its pin before writing it.

## Quest

The headset must be connected by `adb` and Echo VR closed on it. `adb` is taken from `--adb`, then `$ADB`, then `PATH`;
a Windows `adb.exe` driven from WSL works (paths are converted). The cards are written to the game's unpacked data
folder on the headset (`/sdcard/readyatdawn/_data/5932408047/rad15/android/`), outside the APK, so no APK rebuild is
needed.

Run `pull` after the data folder is on the headset ([docs/install-quest.md](../../docs/install-quest.md), step 2):

    python3 mods/menu-cards/menu_cards.py quest pull  --work WORK            # stock language package + three small frames
    python3 mods/menu-cards/menu_cards.py quest build --work WORK --out DIR
    python3 mods/menu-cards/menu_cards.py quest push  --out DIR

- `pull` copies the stock inputs off the headset into `WORK` and checks their pins.
- `build` writes `DIR/cards/`, `DIR/stock/` and `DIR/cards.json`, as on PC.
- `push` copies `DIR/cards/` to the headset and checks the sha256 of the copy on the headset.

**Keep `DIR`.** To return to stock:

    python3 mods/menu-cards/menu_cards.py quest revert --out DIR

`revert` pushes `DIR/stock/` back after checking its pin. Uninstalling the APK does not remove the cards, because they
live in the data folder: revert first.

## Errors

Every refusal prints `refused: …; the install was not changed` and exits with code 2.

- *sha256 … is not the stock …*: the input is not the stock file of the target build (another version, or already
  modified).
- *no manifests/48037dc70b0ecab2 under …*: `--game` does not point at the install root.
- *--out must be outside the game install*: pick a work folder elsewhere.
- *the install's language package is neither stock nor this build's cards*: something else changed it. Restore the
  stock file from your own copy of the game, then build again.
- *Echo VR is running on the headset; close it first*: `adb shell am force-stop com.readyatdawn.r15`.
