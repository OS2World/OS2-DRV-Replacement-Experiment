@echo off
rem ---------------------------------------------------------------------------
rem  compile-test.cmd - build ctrytest.exe (the DosQueryCtryInfo cross-check
rem  tool, see ..\PLAN.md Sec 5) and ctrytest2.exe (the DosQueryCollate/
rem  DosMapCase/DosQueryDBCSEnv cross-check tool, see ..\PLAN.md Sec 8).
rem  Run from anywhere on the VM:
rem  D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\COUNTRY\compile-test.cmd
rem
rem  Ordinary 32-bit OS/2 console app, NOT a driver - same wcl386 recipe as
rem  the sibling POINTDD.SYS/KBDBASE.SYS projects' own test apps. No boot
rem  risk at all - COUNTRY.SYS is passive data, this is an ordinary program.
rem  Same self-logging/tee pattern as those projects' compile*.cmd scripts.
rem ---------------------------------------------------------------------------

if "%1"=="_LOGGED_" goto :run

cmd /c D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\COUNTRY\compile-test.cmd _LOGGED_ 2>&1 | tee -a D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\COUNTRY\compile-test.log
goto :end

:run
set WATCOM=C:\WATCOM
set PATH=%WATCOM%\BINP;%WATCOM%\BINW;%PATH%
set INCLUDE=%WATCOM%\H;%WATCOM%\H\OS2

echo.
echo ============================================================
echo  compile-test.cmd run started
echo ============================================================

d:
cd \PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\COUNTRY\test

echo === wcl386 (ctrytest) ===
wcl386 -bt=os2 -l=os2v2 ctrytest.c -fe=ctrytest.exe

echo === wcl386 (ctrytest2) ===
wcl386 -bt=os2 -l=os2v2 ctrytest2.c -fe=ctrytest2.exe

echo.
echo === result ===
dir ctrytest.exe
dir ctrytest2.exe
echo === done ===

:end
