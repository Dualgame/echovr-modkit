# early-quit

Restores the client half of the early-quit system that the final builds shipped but switched off: the quit notice, the
INFO pages, the lockout timer and the EARLY QUIT menu card. The stock client masks the flag byte the host sends
(`byte & 0xdd`), drops the lockout expiry, and fixes "lockout active" to 0, so a stock client can show only INFO 2/2.
The patch keeps every flag bit, keeps the expiry, and computes "active" as "expiry set and in the future". How the
wire works: [docs/architecture.md](../../docs/architecture.md), section 2.

The lockout itself is the host's: the client has no lockout logic. A host must send the early-quit messages, for
example the EchoRelay fork started with `--early-quit true` (its `docs/protocol/early-quit.md`). Without such a host
the patched client behaves like a stock one.

Targets: PC build `6323983201049540` and Quest build `6528386917200980` (version code 4987566). What has been tested,
on which build: [docs/status.md](../../docs/status.md).

## What is in this folder

| path | what | used for |
|---|---|---|
| `pc/earlyquit.c` | `evrmod_earlyquit.dll`, a plugin for the `sdk/pc` loader | **the PC mod.** It patches four sites (23 bytes) in the running game, in memory; `echovr.exe` on disk stays stock |
| `quest/quest_libr15_earlyquit_patch.py` | six sites (24 bytes) in a copy of the Quest game library `libr15.so` | **the Quest mod.** The APK builder (`sdk/quest/quest_apk_build_release.py`) applies it as step 5; there is nothing to run by hand |
| `tools/pc_exe_earlyquit_patch.py` | the same four PC sites, written into a copy of `echovr.exe` | a reference and check tool only. It is not part of the PC install |

## PC

Built with the other plugins from a Visual Studio x64 developer prompt:

    sdk\pc\build_mods.cmd

Copy `evrmod_earlyquit.dll` into the `mods\` folder beside `echovr.exe`, next to the `winmm.dll` loader
([docs/install-pc.md](../../docs/install-pc.md)). Before writing anything, the plugin checks the build and all four
sites: if every site is stock it patches all four; if every site is already patched it does nothing; anything else is
refused and nothing is written. It logs one line per site to `_local\earlyquit.log` beside the executable.

Uninstall: delete `mods\evrmod_earlyquit.dll`. Nothing on disk was changed apart from the log.

## Quest

The builder applies the six sites to its copy of `libr15.so` every time it builds an APK, after checking the stock
bytes ([docs/install-quest.md](../../docs/install-quest.md)). To return to stock, reinstall the APK you used before.

The script can also inspect or revert a copy of the library by itself. `STOCK/libr15.so` is the library from your
stock APK (`unzip -p STOCK.apk lib/arm64-v8a/libr15.so > STOCK/libr15.so`):

    python3 mods/early-quit/quest/quest_libr15_earlyquit_patch.py --so COPY/libr15.so --stock STOCK/libr15.so --status
    python3 mods/early-quit/quest/quest_libr15_earlyquit_patch.py --so COPY/libr15.so --stock STOCK/libr15.so --revert

Every byte outside the six sites must equal the stock library, or the script refuses.

## The PC file tool (reference only)

`tools/pc_exe_earlyquit_patch.py` writes the same four sites into a **copy** of `echovr.exe` and can revert or report
them. It exists to check the in-memory patch against a file you can compare byte for byte. It refuses an executable
that looks like a live install (a folder with `_data` or `manifests` near it), anything under a `--protect DIR`, a
hardlink, and the stock file itself, and it requires every other byte to equal the stock executable.

    python mods/early-quit/tools/pc_exe_earlyquit_patch.py --exe COPY/echovr.exe --stock STOCK/echovr.exe --status
    python mods/early-quit/tools/pc_exe_earlyquit_patch.py --exe COPY/echovr.exe --stock STOCK/echovr.exe
    python mods/early-quit/tools/pc_exe_earlyquit_patch.py --exe COPY/echovr.exe --stock STOCK/echovr.exe --revert

If the plugin runs in a game whose executable is already file-patched, it finds all four sites patched and does
nothing.
