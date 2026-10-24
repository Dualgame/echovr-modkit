@echo off
rem Build rad-links.dll (x64, static CRT). Needs the MSVC x64 toolchain; set VCVARS if vcvars64.bat lives elsewhere.
rem EVRMOD_INC = the directory holding evrmod.h and evrmod_profile_pc.h (override it if your layout differs).
rem No debug info unless DEBUG=1 is set. Load the DLL through the evrmod winmm.dll loader, or inject it yourself.
setlocal
if not defined VCVARS set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
cd /d "%~dp0"
if not defined EVRMOD_INC set "EVRMOD_INC=..\..\sdk\pc\include"
if not exist build mkdir build
set "CFLAGS=/nologo /O2 /W4 /MT /GS- /D_CRT_SECURE_NO_WARNINGS /I%EVRMOD_INC% /Fobuild\"
set "LINKFLAGS="
if defined DEBUG set "CFLAGS=%CFLAGS% /Zi /Fdbuild\"
if defined DEBUG set "LINKFLAGS=/link /DEBUG"
cl %CFLAGS% /LD src\radlinks.c /Fe:build\rad-links.dll %LINKFLAGS% || exit /b 1
echo BUILD OK
