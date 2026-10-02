@echo off
rem ---------------------------------------------------------------------------
rem  compile.cmd - build PTRSKEL.OS2 (Phase 1) with the 16-bit DDK toolchain.
rem  Run from anywhere on the VM: D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\POINTDD\compile.cmd
rem
rem  Writes compile.log (appended each run, same convention as ..\..\Util\
rem  WarpJoy\compile.cmd) AND shows the same output live on the console - via
rem  tee (C:\usr\bin\tee.exe, confirmed present and already on PATH).
rem
rem  A .cmd can't pipe its OWN output from inside a single pass, so this
rem  re-invokes itself once with a marker argument, piping THAT invocation
rem  through tee; the real build steps only run on the second (marked) pass.
rem
rem  Toolchain sourced from LOCAL VM paths (confirmed via a full `dir C:\ /s`,
rem  2026-09-30 - see ..\PLAN.md Sec 3a for why each one, incl. two lookalikes
rem  that turned out to be the WRONG tool under the right name):
rem    - C:\MiniDDK\base\{h,inc,lib}     - current/maintained DDK headers+libs
rem    - ...DDK_2004\DDK\ZIP\MASM60\binb - MASM.EXE+LINK.EXE, a matched pair
rem    - C:\usr\bin                     - cl.exe/nmake.exe (last on PATH, so
rem                                        its bogus link.exe is never found
rem                                        before the real one above)
rem ---------------------------------------------------------------------------

if "%1"=="_LOGGED_" goto :run

rem %~f0 (NT cmd.exe's "full path of this script") does NOT exist in OS/2's
rem CMD.EXE - use the known fixed project path directly instead. No "call"
rem needed either (that's only for invoking a script from within an already-
rem running batch to return control after - this is a fresh subshell).
cmd /c D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\POINTDD\compile.cmd _LOGGED_ 2>&1 | tee -a D:\PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\POINTDD\compile.log
goto :end

:run
set DDKB=C:\MiniDDK\base
set MASMTOOLS=C:\Desktop\DDK\IBM_OS2_DDK_2004\DDK\ZIP\MASM60\binb

set INCLUDE=%DDKB%\h;%DDKB%\inc
set LIB=%DDKB%\lib
set PATH=%MASMTOOLS%;C:\usr\bin;%PATH%

echo.
echo ============================================================
echo  compile.cmd run started
echo ============================================================
echo === toolchain ===
echo INCLUDE=%INCLUDE%
echo LIB=%LIB%
echo.

d:
cd \PROJECTS\Drivers\OS2-DRV-Replacement-Experiment\POINTDD\src

echo === nmake ===
nmake

echo.
echo === result ===
dir PTRSKEL.OS2
echo === done ===

:end
