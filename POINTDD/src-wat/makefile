# makefile - POINTDD clone, Phase 1 skeleton (OpenWatcom + Drv16Kit)
#
# Adapted from the Drv16Kit's own Required\makefile (a proven template - the
# kit's ReadMe notes some MultiMac drivers already build with it), trimmed to
# build just our one driver (PTRSKEL.sys, from PTRSKEL.c) instead of the
# sample's Driver.sys + BaseDev.sys pair.
#
# Requires WATCOM, DDK, and DRV16KIT set in the environment (compile-wat.cmd,
# at the project root, sets all three before invoking wmake here).

!ifndef %WATCOM
!error WATCOM must be set in the environment.
!endif
WATCOM=$(%WATCOM)

!ifndef %DDK
!error DDK must be set in the environment.
!endif
DDK=$(%DDK)

!ifndef %DRV16KIT
!error DRV16KIT must be set in the environment.
!endif
DRV16KIT=$(%DRV16KIT)

VENDOR=POINTDD clone project
VERSION=0.01

.ERASE

.SUFFIXES:
.SUFFIXES: .sys .exe .obj .asm .inc .def .lrf .lst .sym .map .cpp .c .h .lib .dbg .dll .itl .hlp

%PATH=$(DRV16KIT);$(DDK)\base\TOOLS;$(WATCOM)\BINP;$(WATCOM)\BINW;
%INCLUDE=.;$(DRV16KIT);$(WATCOM)\H;$(DDK)\base\h;$(DDK)\base\inc;
%LIB=$(WATCOM)\lib386;$(WATCOM)\lib386\os2;$(DDK)\base\LIB;

CC=wcc
LINK=wlink
C16DRVFLAGS=-q -bt=os2 -6 -obilmr -s -ze -zl -zq -zff -zgf -ms -wx -zp4 -za99 -zu

# Inference rules
.c.obj: .AUTODEPEND
  $(CC) $(C16DRVFLAGS) $*.c

all: PTRSKEL.sys

.obj.sys: makefile $(DRV16KIT)\Drv16.lib $(DDK)\base\lib\os2286.lib
  @%create $^*.lrf
  @%append $^*.lrf system os2 dll
  @%append $^*.lrf option quiet,map,verbose,caseexact,eliminate sort global
  @%append $^*.lrf name $^*.sys
  @%append $^*.lrf file $(DRV16KIT)\Drv16.lib(header)
  @%append $^*.lrf file $[@
  @%append $^*.lrf library $(DRV16KIT)\Drv16.lib
  @%append $^*.lrf library $(DDK)\BASE\lib\rmcalls.lib
  @%append $^*.lrf library $(DDK)\base\lib\os2286.lib
  $(LINK) @$^*.lrf
  @%erase $^*.lrf

clean: .symbolic
  @if exist *.map @del *.map
  @if exist *.lst @del *.lst
  @if exist *.obj @del *.obj
  @if exist *.sys @del *.sys
  @if exist *.sym @del *.sym
