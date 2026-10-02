@echo off
rem ---------------------------------------------------------------------------
rem  compile-wat.cmd - build KBDBASE.SYS (Phase 1) with OpenWatcom + Drv16Kit.
rem  Run from anywhere on the VM:
rem  D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\KBDBASE\compile-wat.cmd
rem
rem  Same self-logging/tee pattern as the sibling POINTDD.SYS project's
rem  compile-wat.cmd - writes compile-wat.log (appended each run) AND shows
rem  output live via tee, re-invoking itself once internally since a .cmd
rem  can't pipe its own output in one pass. No %~f0, no "goto :eof" (NT-only
rem  cmd.exe extensions OS/2's CMD.EXE doesn't support) - known fixed path
rem  and a real :end label instead.
rem
rem  *** This builds KBDBASE.SYS - the REAL, kernel-hardcoded driver name.
rem  There is no decoy-name build for this project (see ..\PLAN.md Sec 1) -
rem  every boot test of this output IS a real-name test. Take a fresh
rem  VirtualBox snapshot before loading this on any VM, every time. ***
rem ---------------------------------------------------------------------------

if "%1"=="_LOGGED_" goto :run

cmd /c D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\KBDBASE\compile-wat.cmd _LOGGED_ 2>&1 | tee -a D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\KBDBASE\compile-wat.log
goto :end

:run
set WATCOM=C:\WATCOM
set DDK=C:\MiniDDK
set DRV16KIT=C:\Drv16\Kit

echo.
echo ============================================================
echo  compile-wat.cmd run started
echo ============================================================
echo === environment ===
echo WATCOM=%WATCOM%
echo DDK=%DDK%
echo DRV16KIT=%DRV16KIT%
echo.

d:
cd \PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\KBDBASE\src-wat

echo === wmake ===
wmake

echo.
echo === result ===
dir KBDBASE.SYS
echo === done ===

:end
