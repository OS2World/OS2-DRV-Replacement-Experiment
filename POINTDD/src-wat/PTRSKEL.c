/**
 * PTRSKEL.c - POINTDD clone - OpenWatcom + Drv16Kit port.
 *
 * Named to match the PTRSKEL.sys target (wmake's .c.obj/.obj.sys inference
 * rules chain by matching base filename, not by following the `all:` target
 * - the kit's own sample names its source Driver.c to match Driver.sys for
 * exactly this reason; this file follows the same convention).
 *
 * Phase 1 (DONE, boot-tested successfully): prove the toolchain produces a
 * driver the kernel loads and runs Init/InitComplete on without hanging or
 * trapping the boot. Adapted from David Azarewicz's Drv16Kit sample
 * (References\Drv16\Required\Driver.c) - the kit's own "bare minimum driver"
 * template. Notably, this kit needs NO hand-written MASM device header at
 * all - the Drv16.lib(header) module (linked in via the .lrf, see makefile)
 * supplies the device-driver header/Strategy-dispatch plumbing that
 * ..\src\ptr_hdr.asm had to hand-build from devhdr.inc.
 *
 * Phase 2 (in progress): implement PTR_GETPTRDRAWADDRESS (IOCTL_SCR_AND_
 * PTRDRAW / 0x72) - per PDDREF.pdf, "This function is used by the mouse
 * subsystem to obtain the entry point address of the pointer draw routine
 * ... supported by the physical Pointer Draw device driver" - i.e. WE answer
 * this call, we don't make it. No released source exists anywhere for
 * whatever actually calls this (checked MOUSE.SYS's own source and the rest
 * of the DDK - see PLAN.md Phase 2 notes), so for now this traces every
 * GenIOCtl that arrives and answers 0x72 with a safe no-op placeholder
 * routine, to observe what happens empirically rather than guess further.
 *
 * FIRST ATTEMPT TRAPPED (TRAP 000D / GPF) the first time a real caller
 * (testptr.exe) actually triggered STRATEGY_GENIOCTL post-boot. Root cause:
 * used iprintf() there, assuming its doc ("works for both DEVICE= and
 * BASEDEV=") meant it was safe any time post-init. On closer reading,
 * iprintf wraps DevHelp_Save_Message, whose OWN documentation (PDDREF.pdf)
 * says "the message is not displayed immediately, but is queued until
 * SYSTEM INITIALIZATION retrieves it" - i.e. it's a boot-time-only
 * mechanism too, just usable by both driver types during THEIR init, not a
 * general post-init logger. The trap's DSLIM (0x10a) exactly matched the
 * linker map's boundary between resident data and the discardable
 * _INITDATA segment, consistent with hitting something whose residency
 * wasn't accounted for post-shrink. Fixed by going back to the proven-safe
 * technique from the WiFi RTL8188EU project: raw COM1 UART tracing via
 * PortInByte/PortOutByte (Dev16lib.h - no hand-rolled inline asm needed),
 * which touches no DevHelp/kernel-message machinery at all and is proven
 * safe at any time, including well after boot.
 *
 * Phase 3 step 1 (done, 2026-09-30): prove PtrDrawStub can actually be
 * invoked correctly and receives its parameters right, via
 * PTR_TEST_INVOKE_DRAW - a private, driver-own test-trigger IOCtl (see its
 * own comment) that calls PtrDrawStub directly from within our own 16-bit
 * code, sidestepping the unverified question of whether a 32-bit ring-3 app
 * can safely call a 16:16 far pointer directly (and it's not representative
 * of real usage anyway - MOUSE.SYS, the real caller, is itself 16-bit).
 *
 * Phase 3 step 2 (done, 2026-09-30): proved a real write to the mapped VGA
 * text buffer (physical 0xB8000, via MapPhysToVirt) genuinely persists and
 * reads back correctly - but ONLY from a true full-screen VIO/text session;
 * a PM-windowed command prompt doesn't map to live 0xB8000 at all (confirmed
 * both ways - see PLAN.md Sec 4b). The write mechanism itself is proven.
 *
 * Phase 3 step 3 (starting, 2026-09-30): real cursor draw/erase with
 * save-under. (ptlX, ptlY) are treated as character-CELL coordinates
 * (col, row) on the 80x25 text screen, NOT pixels - this is an assumption,
 * not a confirmed fact about what a real caller sends (no real caller
 * exists yet for PTRSKEL$ - see Phase 2 notes on why no reference
 * implementation of the orchestration layer exists). The cursor is drawn by
 * swapping the foreground/background nibble of whatever attribute byte was
 * already at the target cell (classic text-mode cursor technique - stays
 * visible against any underlying color without assuming one). Before
 * drawing at a new cell, any previously-drawn cursor cell is restored from
 * its saved original content first ("save-under"), so moving the cursor
 * never permanently clobbers screen content. PTR_TEST_INVOKE_DRAW now cycles
 * through three fixed test cells (the second move changes row as well as
 * column - a genuine multi-row move, not just horizontal), so a single test
 * run exercises the full draw -> move -> erase-and-restore cycle in both
 * directions and reports enough state for testptr.exe to verify each
 * restore was byte-exact. CONFIRMED (2026-09-30): both single-cell and
 * multi-row draw/erase/save-under verified byte-exact, plus visually in a
 * screenshot of the live session.
 *
 * Cleanup on unload (2026-09-30): STRATEGY_DEINSTALL now calls
 * EraseCursor() before returning, so a dynamic driver unload restores
 * whatever's under the cursor rather than leaving a permanently stray
 * highlighted cell on screen. Deliberately NOT done on STRATEGY_CLOSE -
 * that fires on every ordinary DosClose of a handle, not on the driver
 * going away, and a real pointer must stay visible regardless of which app
 * opens/closes the device. NOT empirically tested: a boot-time CONFIG.SYS
 * DEVICE= driver like this one has no straightforward way to trigger a
 * dynamic DEINSTALL (that strategy code exists mainly for hot-pluggable/
 * dynamically-removable drivers) - implemented per the DDK contract for
 * correctness, not yet observed live.
 *
 * Device name is still PTRSKEL$ (the Phase-1 decoy, NOT the real POINTER$
 * name) - stays purely additive alongside the real, untouched POINTDD.SYS/
 * MOUSE.SYS. See ..\PLAN.md Sec 4 for why.
 */
#include "Dev16lib.h"

/* PTR_GETPTRDRAWADDRESS's function code isn't in strategy.h (only the
 * category constants are) - from PDDREF.pdf / bsedev.h (both public/official,
 * not reverse-engineered). Category IOCTL_SCR_AND_PTRDRAW (0x03) is already
 * defined in strategy.h. */
#define PTR_GETPTRDRAWADDRESS  0x72

/* Phase 3: a PRIVATE, driver-own test-trigger function - NOT part of the
 * documented PDDREF.pdf protocol, not a real function any real caller would
 * ever send. 0xF0+ is unused by anything we've seen documented. Lets our own
 * test app (testptr.exe) make the driver call PtrDrawStub ITSELF, from
 * within our own 16-bit code (an ordinary same-segment call), instead of
 * trying to invoke the 16:16 far pointer from 32-bit ring-3 code directly -
 * that would be an unverified cross-bitness ABI question (and not even
 * representative of real usage: the real caller, MOUSE.SYS, is itself
 * 16-bit, so it never needs any 16/32 thunking to call this at all). This
 * sidesteps that question entirely while still validating the draw logic. */
#define PTR_TEST_INVOKE_DRAW   0xF0

/* The PTRDRAWFUNCTION data-packet layout PDDREF.pdf documents for Function
 * 72h (Data Packet Format), cross-confirmed against the real struct in
 * os2tk45\h\bsedev.h:612 (usReturnCode/pfnDraw/pchDataSeg, packed on a 2-byte
 * boundary) - defined locally rather than #including bsedev.h, since that
 * header assumes the full ring-3 os2.h umbrella (INCL_DOSDEVIOCTL etc.),
 * which doesn't mix cleanly with Dev16lib.h's own driver-context types. PFN
 * is already a Drv16Kit type (see strategy.h's init_in.ulDevHlp). */
#pragma pack(2)
typedef struct {
   USHORT usReturnCode;
   PFN    pfnDraw;
   char FAR16DATA *pchDataSeg;
} PTRDRAWFUNCTION;
#pragma pack()

/* Response for PTR_TEST_INVOKE_DRAW (our own private test function, not a
 * documented structure). Phase 3 step 3: reports enough state to verify the
 * draw/erase/save-under cycle end to end from testptr.exe -
 *   curChar/curAttr   - the cursor cell's content right after this draw
 *                        (curAttr should be savedAttr with its nibbles
 *                        swapped)
 *   savedChar/savedAttr - what was AT that cell before we drew over it (the
 *                        save-under this draw just captured, for restoring
 *                        later when the cursor moves away)
 *   oldCellChar/oldCellAttr - if a cursor was already visible somewhere
 *                        else before this call, its cell's content AFTER
 *                        being restored (should exactly match the previous
 *                        call's savedChar/savedAttr if restore is correct).
 *                        0xFF/0xFF sentinel if there was no previous cursor.
 */
#pragma pack(2)
typedef struct {
   USHORT usCallCount;
   long   lastX;
   long   lastY;
   UCHAR  curChar;
   UCHAR  curAttr;
   UCHAR  savedChar;
   UCHAR  savedAttr;
   UCHAR  oldCellChar;
   UCHAR  oldCellAttr;
   USHORT vgaPtrOff;        /* g_vgaText's own offset:selector, so we can */
   USHORT vgaPtrSeg;        /* see what MapPhysToVirt actually returned  */
} PTRTESTRESULT;
#pragma pack()

/* Resolves the forward references from StrategyHandler below. */
void StrategyInit(PREQPACKET prp);

/* Raw COM1 (0x3F8) trace, built on Dev16lib.h's PortInByte/PortOutByte - the
 * same technique proven throughout the WiFi RTL8188EU project, safe at any
 * time (unlike iprintf/cprintf - see the file header comment for why that
 * matters here). Must stay in the permanent code segment, like everything
 * else STRATEGY_GENIOCTL needs post-init. */
void tcom(char c)
{
   unsigned short tries = 0xFFFF;   /* bounded wait so we never hang here */
   while (tries-- && !(PortInByte(0x3FD) & 0x20))   /* LSR bit5 = THR empty */
      ;
   PortOutByte(0x3F8, (unsigned char)c);
}

void tcom_h8(unsigned char b)
{
   static char hx[] = "0123456789ABCDEF";
   tcom(hx[(b >> 4) & 0x0F]);
   tcom(hx[b & 0x0F]);
}

/* Resident state PtrDrawStub records into - must live in the PERMANENT data
 * area (declared here, before the _initdata pragma switch below), same
 * residency reasoning as the code itself: this gets read back long after
 * boot, via PTR_TEST_INVOKE_DRAW. */
USHORT g_drawCallCount = 0;
long   g_lastX = 0;
long   g_lastY = 0;

/* 16:16 far pointer to the mapped VGA text-mode buffer (physical 0xB8000),
 * set up once in StrategyInit via Dev16lib.h's MapPhysToVirt ("creates a
 * PERMANENT mapping" per its own doc - meant to be obtained once and
 * reused, not re-mapped on every draw). NULL until Init runs; PtrDrawStub
 * checks before using it. Resident (declared here, not after the _initdata
 * switch) since it's read long after boot. */
unsigned char far *g_vgaText = 0;

/* Phase 3 step 3: save-under state for the cursor cell currently drawn (if
 * any). g_cursorVisible is 0 until the first successful draw. Resident -
 * read/written long after boot, every time the cursor moves. */
UCHAR  g_cursorVisible = 0;
USHORT g_cursorRow = 0;
USHORT g_cursorCol = 0;
UCHAR  g_savedChar = 0;
UCHAR  g_savedAttr = 0;

/* Text screen is a fixed, well-known 80x25 for the VGA modes this targets
 * (VBoxVideo/SVGA default text mode - see PLAN.md Sec 1 scope decision).
 * Bounds-checking against this BEFORE touching g_vgaText matters: the
 * MapPhysToVirt selector's limit covers exactly the 4000 bytes requested
 * (80*25*2), so an out-of-range cell computes an offset past that limit,
 * which GPFs - there is no soft failure to rely on here. */
#define VGA_COLS   80
#define VGA_ROWS   25
#define VGA_CELL_OFFSET(row, col) \
   (((USHORT)(row) * VGA_COLS + (USHORT)(col)) * 2)

/* Classic text-mode cursor technique: swap the attribute byte's foreground/
 * background nibbles, so the cursor cell stays visible against whatever
 * color was already there instead of assuming a fixed highlight color. */
UCHAR CursorAttrFor(UCHAR attr)
{
   return (UCHAR)((attr << 4) | (attr >> 4));
}

/* Restores whatever was at the current cursor cell before it was drawn, and
 * marks the cursor not-visible. Safe to call even when nothing is currently
 * drawn (checks g_cursorVisible first) - PtrDrawStub always calls this
 * before drawing at a new cell, so moving the cursor never permanently
 * clobbers screen content. */
void EraseCursor(void)
{
   if (g_vgaText && g_cursorVisible) {
      USHORT off = VGA_CELL_OFFSET(g_cursorRow, g_cursorCol);
      g_vgaText[off]     = g_savedChar;
      g_vgaText[off + 1] = g_savedAttr;
      g_cursorVisible = 0;
   }
}

/* Saves whatever is at (row, col) - the "save-under" - then draws the
 * cursor there. Caller (PtrDrawStub) must erase any previous cursor
 * position first, and must have already bounds-checked row/col. */
void DrawCursorAt(USHORT row, USHORT col)
{
   if (g_vgaText) {
      USHORT off = VGA_CELL_OFFSET(row, col);
      g_savedChar = g_vgaText[off];
      g_savedAttr = g_vgaText[off + 1];
      g_vgaText[off + 1] = CursorAttrFor(g_savedAttr);
      g_cursorRow = row;
      g_cursorCol = col;
      g_cursorVisible = 1;
   }
}

/* Phase 2 placeholder pointer-draw routine: the address we hand out via
 * PTR_GETPTRDRAWADDRESS. Does nothing yet (real drawing is Phase 3 - this
 * step only proves/observes the handshake) - so it must be completely safe
 * to call from an unknown context: no OS/DevHelp calls, no assumptions.
 * Signature matches pointer.asm's documented draw_pointer(LONG ptlX, LONG
 * ptlY) (References\os2ddk2004\...\vga32\svga256\pointer.asm:278) since our
 * routine plays the same conceptual role for whoever calls it - matching the
 * expected calling convention avoids a stack mismatch if this DOES get
 * called for real before we understand more. Must stay in the PERMANENT
 * (non-discardable) code segment, same as StrategyHandler - unlike
 * StrategyInit, this can be called at any time long after boot, so it must
 * NOT end up in the "_inittext" segment the kernel discards after init. */
/* No #pragma aux here (unlike StrategyHandler below): that's only needed to
 * force an unusual calling convention when we KNOW the caller's fixed ABI
 * (the kernel's ES:BX for Strategy). We don't yet know for certain what
 * convention calls this - pointer.asm's draw_pointer(LONG,LONG) is our best
 * evidence, not a proven fact for this path - so this stays a plain `far` C
 * function with the compiler's normal parameter passing rather than a
 * guessed-and-possibly-wrong custom one. */
void far PtrDrawStub(long ptlX, long ptlY)
{
   g_drawCallCount++;
   g_lastX = ptlX;
   g_lastY = ptlY;

   /* Bounds-check against the real 80x25 text screen BEFORE touching video
    * memory - an out-of-range cell would GPF (see VGA_CELL_OFFSET's
    * comment). (ptlX, ptlY) are assumed to be (col, row) character-cell
    * coordinates, not pixels - unconfirmed against any real caller (see
    * file header) - PTR_TEST_INVOKE_DRAW always sends in-range values for
    * this reason. Silently ignoring an out-of-range request (rather than
    * clamping it) is deliberate: we don't yet know enough about real
    * caller behavior to guess what clamping should look like. */
   if (ptlX < 0 || ptlX >= VGA_COLS || ptlY < 0 || ptlY >= VGA_ROWS)
      return;

   EraseCursor();                                    /* restore old cell, if any */
   DrawCursorAt((USHORT)ptlY, (USHORT)ptlX);          /* save-under + draw new cell */
}

/* The kernel calls this whenever it wants to access the driver - must be
 * declared exactly this way (far, ES:BX parm) per the kit's own doc. */
#pragma aux StrategyHandler far parm [es bx];
void StrategyHandler(PREQPACKET prp)
{
   /* Default status unless changed below - must always have RPDONE set. */
   prp->usStatus = RPDONE;

   switch (prp->bCommand) {

   case STRATEGY_INIT:
      /* Called at ring 3, immediately after load. */
      StrategyInit(prp);
      break;

   case STRATEGY_INITCOMPLETE:
      /* Called after every driver in the system has loaded. Phase 1 does
       * nothing here beyond the kit's own required call. */
      Drv16InitComplete();
      break;

   case STRATEGY_OPEN:
   case STRATEGY_CLOSE:
   case STRATEGY_SAVERESTORE:
      /* Not needed yet - default RPDONE above already covers these.
       * STRATEGY_CLOSE deliberately does NOT erase the cursor: it fires on
       * every DosClose of a handle (normal per-handle I/O lifecycle), not on
       * the driver going away - a real pointer must stay visible regardless
       * of which app opens/closes the device. Only STRATEGY_DEINSTALL below
       * (the driver actually being removed from memory) warrants cleanup. */
      break;

   case STRATEGY_DEINSTALL:
      /* The driver is being removed from memory - restore whatever's under
       * the cursor now, before this code/data becomes invalid, so a dynamic
       * unload doesn't leave a permanently stray highlighted cell on
       * screen. Safe to call even if nothing was ever drawn (EraseCursor
       * checks g_cursorVisible itself). */
      EraseCursor();
      break;

   case STRATEGY_GENIOCTL:
      /* Phase 2: trace every GenIOCtl that arrives (category/function), so
       * we can observe whatever calls this decoy-named driver at all - via
       * the COM1 tcom() trace (see its comment for why, not iprintf/cprintf,
       * both boot-time-only mechanisms). 'G' cat:func */
      tcom('G');
      tcom_h8(prp->ioctl.bCategory);
      tcom(':');
      tcom_h8(prp->ioctl.bFunction);

      if (prp->ioctl.bCategory == IOCTL_SCR_AND_PTRDRAW &&
          prp->ioctl.bFunction == PTR_GETPTRDRAWADDRESS) {
         if (prp->ioctl.usDataLen < sizeof(PTRDRAWFUNCTION)) {
            prp->usStatus = RPDONE | RPERR | RPERR_LENGTH;
            tcom('s');   /* short buffer */
         } else {
            PTRDRAWFUNCTION FAR16DATA *pResp =
               (PTRDRAWFUNCTION FAR16DATA *)prp->ioctl.pvData;
            pResp->usReturnCode = 0;
            pResp->pfnDraw = (PFN)PtrDrawStub;
            pResp->pchDataSeg = (char FAR16DATA *)0;
            tcom('R');   /* answered PTR_GETPTRDRAWADDRESS */
         }
      } else if (prp->ioctl.bCategory == IOCTL_SCR_AND_PTRDRAW &&
                 prp->ioctl.bFunction == PTR_TEST_INVOKE_DRAW) {
         /* Phase 3 step 3 test-only trigger - see PTR_TEST_INVOKE_DRAW's
          * comment. Cycles through three fixed test cells so successive
          * calls exercise the full draw -> move -> erase-and-restore cycle:
          * position 0 -> 1 is a same-row move (column only), position
          * 1 -> 2 changes BOTH row and column - a genuine multi-row/
          * diagonal move, proving the save-under logic isn't accidentally
          * relying on row staying fixed (VGA_CELL_OFFSET already generalizes
          * to any row/col, but this is the first test to actually exercise
          * a row change). Captures the previous cursor's position BEFORE
          * calling PtrDrawStub (which erases it) so the response can report
          * what that cell looks like AFTER being restored. */
         static const long testCol[3] = { 10, 20, 15 };
         static const long testRow[3] = {  5,  5, 10 };
         long idx = g_drawCallCount % 3;
         long useCol = testCol[idx];
         long useRow = testRow[idx];
         UCHAR hadPrevious = g_cursorVisible;
         USHORT prevRow = g_cursorRow;
         USHORT prevCol = g_cursorCol;

         PtrDrawStub(useCol, useRow);

         if (prp->ioctl.usDataLen < sizeof(PTRTESTRESULT)) {
            prp->usStatus = RPDONE | RPERR | RPERR_LENGTH;
            tcom('s');
         } else {
            PTRTESTRESULT FAR16DATA *pResp =
               (PTRTESTRESULT FAR16DATA *)prp->ioctl.pvData;
            /* Extract g_vgaText's segment:offset via a union, not a shift on
             * a cast pointer - far-pointer-to-integer conversion semantics
             * are implementation-defined, so this uses the same "read the
             * raw bytes through a union" technique already proven
             * throughout this project rather than risk a wrong/misleading
             * value from a cast+shift that may not do what it looks like. */
            {
               union { unsigned char far *p; USHORT w[2]; } u;
               u.p = g_vgaText;
               pResp->vgaPtrOff = u.w[0];
               pResp->vgaPtrSeg = u.w[1];
            }
            pResp->usCallCount = g_drawCallCount;
            pResp->lastX = g_lastX;
            pResp->lastY = g_lastY;

            if (g_vgaText) {
               USHORT curOff = VGA_CELL_OFFSET(g_cursorRow, g_cursorCol);
               pResp->curChar = g_vgaText[curOff];
               pResp->curAttr = g_vgaText[curOff + 1];
            } else {
               pResp->curChar = 0xFF;
               pResp->curAttr = 0xFF;
            }
            pResp->savedChar = g_savedChar;
            pResp->savedAttr = g_savedAttr;

            if (hadPrevious && g_vgaText) {
               USHORT prevOff = VGA_CELL_OFFSET(prevRow, prevCol);
               pResp->oldCellChar = g_vgaText[prevOff];
               pResp->oldCellAttr = g_vgaText[prevOff + 1];
            } else {
               pResp->oldCellChar = 0xFF;   /* sentinel: no previous cursor */
               pResp->oldCellAttr = 0xFF;
            }
            tcom('T');   /* test-invoke done, result written */
         }
      } else {
         /* Anything else: not yet handled. */
         prp->usStatus = RPDONE | RPERR | RPERR_BADCOMMAND;
         tcom('x');
      }
      break;

   default:
      prp->usStatus = RPDONE | RPERR | RPERR_GENERAL;
   }
}

/* Discardable after init - everything below goes into the init-only
 * text/data segments the kernel throws away once loading finishes. */
#pragma code_seg ("_inittext");
#pragma data_seg ("_initdata","endds");

char cDevName[9] = "PTRSKEL$";   /* Phase-1 decoy name - see file header */

void StrategyInit(PREQPACKET prp)
{
   short Success;

   UtSetDriverName(cDevName);

   Success = 1;
   if (Drv16Init(prp)) Success = 0;

   if (Success) {
      prp->usStatus = RPDONE;
      prp->init_out.usCodeEnd = (USHORT) _TextEnd;
      prp->init_out.usDataEnd = (USHORT) &_DataEnd;

      /* Phase 3 step 2: map the VGA text-mode buffer (physical 0xB8000,
       * 4000 bytes = one full 80x25 screen) ONCE here - MapPhysToVirt's own
       * doc says it "creates a PERMANENT mapping", so this is the right
       * place for it, not PtrDrawStub (which would re-map on every call).
       * g_vgaText stays 0/NULL if this fails, and PtrDrawStub checks before
       * using it - no crash either way. */
      g_vgaText = (unsigned char far *) MapPhysToVirt(0xB8000UL, 4000UL);

      cprintf("POINTDD clone (Phase 1, OpenWatcom): %s loaded OK.\n", cDevName);
   } else {
      prp->usStatus = RPDONE | RPERR;
      prp->init_out.usCodeEnd = 0;
      prp->init_out.usDataEnd = 0;
      cprintf("POINTDD clone (Phase 1, OpenWatcom): %s init FAILED.\n", cDevName);
   }
}
