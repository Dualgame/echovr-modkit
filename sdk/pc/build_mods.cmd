@echo off
rem Build the plugins into build\mods\: rad-links.dll (one binary for both modes: broker plugin under winmm.dll ABI >= 2,
rem self-hooking standalone otherwise) and evrmod_earlyquit.dll, then run the offline broker self-test.
rem x64, static CRT. Needs the MSVC x64 toolchain; set VCVARS if needed. No debug info unless DEBUG=1 is set.
rem EVRMOD_INC (the directory with evrmod.h), RADLINKS_SRC and EARLYQUIT_SRC may be set to override the defaults below.
setlocal
if not defined VCVARS set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
cd /d "%~dp0"
if not defined EVRMOD_INC set "EVRMOD_INC=include"
if not defined RADLINKS_SRC set "RADLINKS_SRC=..\..\mods\rad-links\src\radlinks.c"
if not defined EARLYQUIT_SRC set "EARLYQUIT_SRC=..\..\mods\early-quit\pc\earlyquit.c"
if not exist build\obj mkdir build\obj
if not exist build\mods mkdir build\mods
set "CFLAGS=/nologo /O2 /W4 /MT /GS- /D_CRT_SECURE_NO_WARNINGS /I%EVRMOD_INC% /Fobuild\obj\"
set "LINKFLAGS="
if defined DEBUG set "CFLAGS=%CFLAGS% /Zi /Fdbuild\obj\"
if defined DEBUG set "LINKFLAGS=/link /DEBUG"
cl %CFLAGS% /LD %RADLINKS_SRC% /Fe:build\mods\rad-links.dll %LINKFLAGS% || exit /b 1
cl %CFLAGS% /LD %EARLYQUIT_SRC% /Fe:build\mods\evrmod_earlyquit.dll %LINKFLAGS% || exit /b 1
rem Offline broker self-test (fake image, no game): dispatch order, fault guard, install-once, chaining, load order.
cl /nologo /O2 /W4 /MT /GS- /D_CRT_SECURE_NO_WARNINGS /Fobuild\obj\ tools\broker_selftest.c /Fe:build\broker_selftest.exe || exit /b 1
build\broker_selftest.exe || exit /b 1
del /q build\mods\*.exp build\mods\*.lib build\mods\*.ilk 2>nul
echo BUILD OK
