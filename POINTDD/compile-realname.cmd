@echo off
rem ---------------------------------------------------------------------------
rem  compile-realname.cmd - build PDCLONE.SYS, the REAL-NAME (POINTER$)
rem  swap-in test build. Run: D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\POINTDD\compile-realname.cmd
rem
rem  *** This builds the driver that registers as POINTER$, the real device
rem  name - see src-wat-realname\PDCLONE.c's file header and the swap-in
rem  test procedure before ever loading this on any VM. It must REPLACE the
rem  real POINTDD.SYS's CONFIG.SYS line, never sit alongside it. ***
rem
rem  Same self-logging/tee pattern as the other compile-*.cmd scripts here.
rem ---------------------------------------------------------------------------

if "%1"=="_LOGGED_" goto :run

cmd /c D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\POINTDD\compile-realname.cmd _LOGGED_ 2>&1 | tee -a D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\POINTDD\compile-realname.log
goto :end

:run
set WATCOM=C:\WATCOM
set DDK=C:\MiniDDK
set DRV16KIT=C:\Drv16\Kit

echo.
echo ============================================================
echo  compile-realname.cmd run started
echo ============================================================
echo === environment ===
echo WATCOM=%WATCOM%
echo DDK=%DDK%
echo DRV16KIT=%DRV16KIT%
echo.

d:
cd \PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\POINTDD\src-wat-realname

echo === wmake ===
wmake

echo.
echo === result ===
dir PDCLONE.SYS
echo === done ===

:end
