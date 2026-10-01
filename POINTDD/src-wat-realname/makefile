# makefile - POINTDD clone, REAL-NAME swap-in test build (registers as POINTER$)
#
# Same pattern as ..\src-wat\makefile - see that file's comments for the full
# rationale of every flag/step. Builds PDCLONE.sys from PDCLONE.c (source
# must match target base filename for wmake's inference-rule chaining).

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

VENDOR=POINTDD clone project (swap-in test build)
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

.c.obj: .AUTODEPEND
  $(CC) $(C16DRVFLAGS) $*.c

all: PDCLONE.sys

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
