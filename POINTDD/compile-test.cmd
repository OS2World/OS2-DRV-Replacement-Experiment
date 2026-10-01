@echo off
rem ---------------------------------------------------------------------------
rem  compile-test.cmd - build testptr.exe (the ring-3 IOCtl test app).
rem  Run from anywhere on the VM: D:\PROJECTS\DRIVERS\POINTDD\compile-test.cmd
rem
rem  Ordinary 32-bit OS/2 console app, NOT a driver - per the top-level
rem  CLAUDE.md's "Build a console (VIO) app" recipe: wcl386 -bt=os2 -l=os2v2.
rem  Same self-logging/tee pattern as compile.cmd/compile-wat.cmd (writes
rem  compile-test.log, shows output live, avoids the two NT-only cmd.exe
rem  extensions OS/2 doesn't support).
rem ---------------------------------------------------------------------------

if "%1"=="_LOGGED_" goto :run

cmd /c D:\PROJECTS\DRIVERS\POINTDD\compile-test.cmd _LOGGED_ 2>&1 | tee -a D:\PROJECTS\DRIVERS\POINTDD\compile-test.log
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
cd \PROJECTS\DRIVERS\POINTDD\test

echo === wcl386 ===
wcl386 -bt=os2 -l=os2v2 testptr.c -fe=testptr.exe

echo.
echo === result ===
dir testptr.exe
echo === done ===

:end
