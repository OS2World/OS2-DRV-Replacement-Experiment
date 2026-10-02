@echo off
rem ---------------------------------------------------------------------------
rem  compile-build.cmd - build ctybuild.exe (the COUNTRY.SYS generator,
rem  see PLAN.md Sec 8). Run from anywhere on the VM:
rem  D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\COUNTRY\compile-build.cmd
rem
rem  Ordinary 32-bit OS/2 console app, NOT a driver - no boot risk to build
rem  or run this. Same wcl386 recipe as ctrytest.c. Same self-logging/tee
rem  pattern as the other compile*.cmd scripts in this repo.
rem ---------------------------------------------------------------------------

if "%1"=="_LOGGED_" goto :run

cmd /c D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\COUNTRY\compile-build.cmd _LOGGED_ 2>&1 | tee -a D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\COUNTRY\compile-build.log
goto :end

:run
set WATCOM=C:\WATCOM
set PATH=%WATCOM%\BINP;%WATCOM%\BINW;%PATH%
set INCLUDE=%WATCOM%\H;%WATCOM%\H\OS2

echo.
echo ============================================================
echo  compile-build.cmd run started
echo ============================================================

d:
cd \PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\COUNTRY\build

echo === wcl386 ===
wcl386 -bt=os2 -l=os2v2 ctybuild.c cty_data.c -fe=ctybuild.exe

echo.
echo === result ===
dir ctybuild.exe
echo === done ===

:end
