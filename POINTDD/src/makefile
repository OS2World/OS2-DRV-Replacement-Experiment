#***************************************************************************
#  makefile - POINTDD clone, Phase 1 skeleton  ->  PTRSKEL.OS2
#
#  16-bit DDK build (MS C 6.0 + MASM + LINK), same tool flags/libs pattern as
#  the WiFi RTL8188EU driver's makefile - proven on this DDK/toolchain.
#  usbmisc.lib is dropped (Phase 1 never calls GetDS - verified: that symbol
#  is only referenced by the WiFi driver's own USB code, not required by the
#  DDK/linker in general).
#
#  Build:  run build.cmd  (which sets the env then invokes nmake here).
#***************************************************************************

CC     = cl
AS     = masm
LINK   = link

# 16-bit driver compile flags (verbatim from the WiFi driver / USBETHER):
#   /Zp packed structs  /G2s 286 + no stack checks  /Answ driver memory model
#   /Osegli optimize  /W3 warnings  /c compile-only
CFLAGS = /c /Zp /G2s /Answ /W3 /nologo /Osegli

# MASM flags (from the WiFi driver / USBETHER).
AFLAGS = -MX -T -Z

# 16-bit driver link flags + libs. LIB env (set by build.cmd) resolves these.
LFLAGS = /map /nod /exepack /packd /a:16 /far
LIBS   = dhcalls.lib rmcalls.lib os2386.lib

DRIVER = PTRSKEL.OS2
DEF    = ptrskel.def
OBJS   = ptr_hdr.obj strat.obj

all: $(DRIVER)

$(DRIVER): $(OBJS) $(DEF)
	$(LINK) $(LFLAGS) $(OBJS), $(DRIVER), PTRSKEL.map, $(LIBS), $(DEF);

# --- inference rules (cl/masm read INCLUDE from the environment) --------
.c.obj:
	$(CC) $(CFLAGS) $*.c

.asm.obj:
	$(AS) $(AFLAGS) $*.asm;

# --- dependencies ------------------------------------------------------
ptr_hdr.obj: ptr_hdr.asm
strat.obj:   strat.c

clean:
	-del *.obj
	-del $(DRIVER)
	-del PTRSKEL.map
