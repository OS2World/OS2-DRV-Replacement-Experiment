@echo off
rem ---------------------------------------------------------------------------
rem  compile-wat.cmd - build PTRSKEL.SYS (Phase 1) with OpenWatcom + Drv16Kit.
rem  Run from anywhere on the VM: D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\POINTDD\compile-wat.cmd
rem
rem  Parallel build to compile.cmd (the MS C 6.0/MASM/LINK version, which
rem  already builds clean - see PLAN.md Sec 3). This is the open-source-tools
rem  alternative: wcc (16-bit C), wlink, via David Azarewicz's Drv16Kit (a
rem  proven kit - some MultiMac drivers already build with it), instead of
rem  hand-written MASM + the proprietary MS toolchain.
rem
rem  Same self-logging approach as compile.cmd: writes compile-wat.log
rem  (appended each run) AND shows output live via tee - re-invokes itself
rem  once internally since a .cmd can't pipe its own output in one pass.
rem  (Two NT-only cmd.exe extensions do NOT exist in OS/2's CMD.EXE and are
rem  deliberately avoided here, same lesson as compile.cmd: no %~f0, no
rem  "goto :eof" pseudo-label - use the known fixed path and a real :end
rem  label instead.)
rem
rem  Environment (per References\Drv16\ReadMe.txt "Installing the Required
rem  Packages"): WATCOM = the OpenWatcom install, DDK = MiniDDK (the kit's
rem  own ReadMe recommends MiniDDK specifically), DRV16KIT = the Drv16 Kit
rem  directory. All three confirmed present locally on this VM.
rem ---------------------------------------------------------------------------

if "%1"=="_LOGGED_" goto :run

cmd /c D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\POINTDD\compile-wat.cmd _LOGGED_ 2>&1 | tee -a D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\POINTDD\compile-wat.log
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
cd \PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\POINTDD\src-wat

echo === wmake ===
wmake

echo.
echo === result ===
dir PTRSKEL.SYS
echo === done ===

:end
