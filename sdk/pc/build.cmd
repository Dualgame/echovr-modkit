@echo off
rem Build winmm.dll (the echovr-modloader proxy + mod loader), x64, static CRT.
rem Needs the MSVC x64 toolchain; set VCVARS if vcvars64.bat lives elsewhere. Regenerates src\winmm.def
rem from the system winmm with tools\gen_def.py (pefile). Output in build\winmm.dll. No debug info unless DEBUG=1.
setlocal
if not defined VCVARS set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
rem (Re)generate winmm.def + winmm_orig.def unless --no-def. Needs python with pefile on PATH; if that is not
rem available, fall back to the committed .defs (they are checked in). Only a new winmm export would need a regen.
if /i not "%~1"=="--no-def" (
  python tools\gen_def.py gen
  if errorlevel 1 (
    if exist src\winmm.def ( echo [gen_def skipped - no python/pefile; using committed src\winmm.def] ) else ( echo [gen_def failed and no committed src\winmm.def] & exit /b 1 )
  )
)
rem Import library for the forwarder target, so the parity forwarders resolve at link time.
lib /nologo /DEF:src\winmm_orig.def /MACHINE:X64 /OUT:build\winmm_orig.lib || exit /b 1
set "CFLAGS=/nologo /O2 /W4 /MT /GS- /EHa /D_CRT_SECURE_NO_WARNINGS /Fobuild\"
set "LINKFLAGS="
if defined DEBUG set "CFLAGS=%CFLAGS% /Zi /Fdbuild\"
if defined DEBUG set "LINKFLAGS=/DEBUG"
cl %CFLAGS% /LD src\winmm_proxy.c /Fe:build\winmm.dll /link /DEF:src\winmm.def build\winmm_orig.lib %LINKFLAGS% || exit /b 1
echo BUILD OK
