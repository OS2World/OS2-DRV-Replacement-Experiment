# makefile - KBDBASE clone, Phase 1 skeleton (OpenWatcom + Drv16Kit)
#
# Adapted from the Drv16Kit's own Required\makefile, same pattern already
# proven on the sibling POINTDD.SYS project (..\..\POINTDD\src-wat\makefile) -
# see that file's comments for the full rationale of every flag/step.
#
# Requires WATCOM, DDK, and DRV16KIT set in the environment (compile-wat.cmd,
# at the project root, sets all three before invoking wmake here).
#
# Deliberately does NOT link rmcalls.lib (unlike the POINTDD.SYS makefile this
# was adapted from) - it was never actually called from our source, just
# inherited unused from Drv16Kit's generic sample template, so there's no
# reason to keep it regardless. CORRECTION: this was originally removed on
# the theory that it caused the extra NE-header import the "NE header
# comparison" (..\PLAN.md) found - rebuilding without it produced a
# byte-identical binary, proving that theory wrong. The real second import
# (confirmed via KBDBASE.map: DOS16PUTMESSAGE from module MSG) turned out to
# be cprintf() in KBDBASE.c's StrategyInit - see that file's comment where
# it was replaced with a COM1 trace instead.

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

VENDOR=KBDBASE clone project
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

all: KBDBASE.sys

.obj.sys: makefile $(DRV16KIT)\Drv16.lib $(DDK)\base\lib\os2286.lib
  @%create $^*.lrf
  @%append $^*.lrf system os2 dll
  @%append $^*.lrf option quiet,map,verbose,caseexact,eliminate sort global
  @%append $^*.lrf name $^*.sys
  @%append $^*.lrf file $(DRV16KIT)\Drv16.lib(header)
  @%append $^*.lrf file $[@
  @%append $^*.lrf library $(DRV16KIT)\Drv16.lib
  @%append $^*.lrf library $(DDK)\base\lib\os2286.lib
  $(LINK) @$^*.lrf
  @%erase $^*.lrf

clean: .symbolic
  @if exist *.map @del *.map
  @if exist *.lst @del *.lst
  @if exist *.obj @del *.obj
  @if exist *.sys @del *.sys
  @if exist *.sym @del *.sym
