/**
 * PDCLONE.c - POINTDD clone - REAL-NAME test build (registers as POINTER$).
 *
 * *** SWAP-IN TEST ONLY - NOT for routine use. See ..\PLAN.md "swap-in test"
 * notes and the exact procedure the user was given for this build. ***
 *
 * Was identical logic to ..\src-wat\PTRSKEL.c (the safe decoy-name build,
 * which stays untouched and is what routine development keeps using) - the
 * ONLY difference was cDevName: "POINTER$" instead of "PTRSKEL$". Phase 4
 * (below) ported PTRSKEL.c's Phase 3 draw/erase/save-under logic over too,
 * plus NEW instrumentation specific to this real-name build (see Phase 4
 * comment) - this build and PTRSKEL.c are no longer byte-identical, but the
 * core draw/erase/save-under logic is the same, already proven correct
 * under the decoy name (PLAN.md Sec 4b/Phase 3). This build is STILL never
 * loaded at the same time as the real POINTDD.SYS (both would try to claim
 * the same name) - it REPLACES that line in CONFIG.SYS for the duration of
 * the test, never sits alongside it. The file itself is named PDCLONE.SYS
 * (not POINTDD.SYS) specifically so it can never be confused with, or
 * accidentally overwrite, the real driver file on disk - the swap is done
 * entirely by changing which file CONFIG.SYS's DEVICE= line points at,
 * never by touching C:\OS2\BOOT\POINTDD.SYS itself.
 *
 * Purpose (Phase 2, settled): find out whether the real OS/2 system ("the
 * mouse subsystem" - no released source exists showing who this is, see
 * PLAN.md Phase 2 notes) actually calls PTR_GETPTRDRAWADDRESS on whatever
 * driver owns POINTER$.
 *
 * RUN 1 (2026-09-30) CONFIRMED IT DOES: COM1 trace showed 4 real calls to
 * cat3/func0x72 during normal boot. First succeeded; the next 3 failed our
 * own RPERR_LENGTH check (buffer < 10 bytes) - different real callers send
 * different buffer sizes for the same IOCtl. RUN 2 logged the EXACT
 * usDataLen/usParmLen instead of just short-or-not. RUN 3 (clean, single-
 * boot trace, log cleared first) found something more consistent: ALL 3
 * calls that boot used usDataLen=0 exactly, none succeeded.
 *
 * RUN 4 tried answering usDataLen==0 with fake success instead of
 * rejecting it - CRASHED. Not in this module: "Exception in module:
 * VBOXMOUS" (VirtualBox's own mouse-integration driver). It takes success
 * to mean it received a real draw-routine address and tried to use it,
 * even though nothing was actually written into the zero-length buffer.
 * REVERTED back to Runs 1-3's outright rejection, now understood to be
 * correct and necessary, not a limitation. Strong resulting hypothesis:
 * VBOXMOUS probes POINTER$ for real pointer-draw integration, the probe is
 * SUPPOSED to fail/be rejected, and VBox's own mouse-integration mechanism
 * - not real POINTDD delegation - is what actually draws the visible
 * cursor in this environment (explaining why the pointer looked fine in
 * every run despite PtrDrawStub doing nothing at all).
 *
 * RUN 5 CONFIRMED FIXED: reverted logic boots clean, stable, repeatable.
 * Real-name swap-in investigation closed out for Phase 2's own question.
 *
 * Phase 4 (starting, 2026-09-30): now that PTRSKEL.c's Phase 3 work has
 * PROVEN (under the decoy name, driven by our own private test IOCtl) that
 * the draw/erase/save-under mechanism genuinely writes and restores real
 * VGA text-mode VRAM correctly - including across row changes - the
 * remaining open question is whether the REAL system (MOUSE.SYS, via
 * whatever obtained our pfnDraw from Run 1's successful GETPTRDRAWADDRESS
 * call) ever actually CALLS that address during real mouse movement, or
 * whether VBOXMOUS's own fallback mechanism (per the Run 4 finding above)
 * means it never does. This build ports the proven draw/erase/save-under
 * logic over from PTRSKEL.c (bounds-checked to the real 80x25 text screen,
 * same as there), and - since there is no ring-3 test app driving this
 * build the way testptr.exe drives PTRSKEL$ - adds a COM1 trace at the TOP
 * of PtrDrawStub itself, logging the raw (ptlX, ptlY) received (full 32-bit
 * hex, BEFORE the bounds-check) and whether the call was accepted (in
 * range, drawn) or ignored (out of range) - 'D' marker + 8 hex digits of
 * ptlX + ':' + 8 hex digits of ptlY + 'Y' or 'N'. This is the ONLY way to
 * observe a real invocation from this build, and logging the raw value
 * before bounds-checking also answers a second open question for free:
 * whether a real caller sends character-CELL coordinates (what we assume,
 * 0-79/0-24) or PIXEL coordinates (likely out of that range, and the
 * genuinely documented unit for pointer.asm's graphics-mode draw_pointer,
 * whose signature we copied without confirming its unit applies here too -
 * see PTRSKEL.c's own file header on this unconfirmed assumption).
 *
 * This build still only implements Init/InitComplete/GENIOCTL(0x72) +
 * PtrDrawStub - nothing else POINTER$ is normally responsible for (no
 * native VIO-mode pointer drawing beyond PtrDrawStub itself, none of the
 * MOU_* category 7 surface - that's MOUSE$'s own logical-driver interface,
 * a different driver entirely, not something POINTDD.SYS implements; see
 * PLAN.md's Phase 4 scope clarification). Realistic worst case if something
 * depends on those: a broken/invisible mouse pointer for the test session,
 * not a hang - Init/InitComplete themselves are the exact logic already
 * boot-tested safely under the decoy name, and PtrDrawStub's bounds-check
 * means an unexpected coordinate range can never GPF. But this is
 * genuinely less proven than the decoy-name test, which is why this stays
 * a deliberate swap-in/observe/swap-back experiment with a CONFIG.SYS
 * backup, not a permanent change.
 */
#include "Dev16lib.h"

#define PTR_GETPTRDRAWADDRESS  0x72

#pragma pack(2)
typedef struct {
   USHORT usReturnCode;
   PFN    pfnDraw;
   char FAR16DATA *pchDataSeg;
} PTRDRAWFUNCTION;
#pragma pack()

void StrategyInit(PREQPACKET prp);

void tcom(char c)
{
   unsigned short tries = 0xFFFF;
   while (tries-- && !(PortInByte(0x3FD) & 0x20))
      ;
   PortOutByte(0x3F8, (unsigned char)c);
}

void tcom_h8(unsigned char b)
{
   static char hx[] = "0123456789ABCDEF";
   tcom(hx[(b >> 4) & 0x0F]);
   tcom(hx[b & 0x0F]);
}

void tcom_h16(unsigned short w)
{
   tcom_h8((unsigned char)(w >> 8));
   tcom_h8((unsigned char)w);
}

/* Phase 4: needed to log PtrDrawStub's full 32-bit (ptlX, ptlY) - neither
 * earlier build ever needed to trace a `long` over COM1 before (testptr.exe
 * read it back structurally instead). */
void tcom_h32(long v)
{
   tcom_h16((unsigned short)((unsigned long)v >> 16));
   tcom_h16((unsigned short)v);
}

/* Resident state - see ..\src-wat\PTRSKEL.c for the full rationale on each
 * of these (ported from there, Phase 3/4). */
unsigned char far *g_vgaText = 0;
UCHAR  g_cursorVisible = 0;
USHORT g_cursorRow = 0;
USHORT g_cursorCol = 0;
UCHAR  g_savedChar = 0;
UCHAR  g_savedAttr = 0;

#define VGA_COLS   80
#define VGA_ROWS   25
#define VGA_CELL_OFFSET(row, col) \
   (((USHORT)(row) * VGA_COLS + (USHORT)(col)) * 2)

UCHAR CursorAttrFor(UCHAR attr)
{
   return (UCHAR)((attr << 4) | (attr >> 4));
}

void EraseCursor(void)
{
   if (g_vgaText && g_cursorVisible) {
      USHORT off = VGA_CELL_OFFSET(g_cursorRow, g_cursorCol);
      g_vgaText[off]     = g_savedChar;
      g_vgaText[off + 1] = g_savedAttr;
      g_cursorVisible = 0;
   }
}

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

/* Phase 4: the real entry point - whatever actually calls this (if
 * anything does - that's the open question this build exists to answer)
 * is NOT under our control the way testptr.exe was, so this traces its OWN
 * invocation unconditionally, first thing, before any bounds-checking -
 * 'D' + 8 hex digits ptlX + ':' + 8 hex digits ptlY, then 'Y' if accepted
 * (in range, drawn) or 'N' if ignored (out of range). */
void far PtrDrawStub(long ptlX, long ptlY)
{
   tcom('D');
   tcom_h32(ptlX);
   tcom(':');
   tcom_h32(ptlY);

   if (ptlX < 0 || ptlX >= VGA_COLS || ptlY < 0 || ptlY >= VGA_ROWS) {
      tcom('N');
      return;
   }
   tcom('Y');

   EraseCursor();
   DrawCursorAt((USHORT)ptlY, (USHORT)ptlX);
}

#pragma aux StrategyHandler far parm [es bx];
void StrategyHandler(PREQPACKET prp)
{
   prp->usStatus = RPDONE;

   switch (prp->bCommand) {

   case STRATEGY_INIT:
      StrategyInit(prp);
      break;

   case STRATEGY_INITCOMPLETE:
      Drv16InitComplete();
      break;

   case STRATEGY_OPEN:
   case STRATEGY_CLOSE:
   case STRATEGY_SAVERESTORE:
      /* STRATEGY_CLOSE deliberately does NOT erase the cursor - see
       * ..\src-wat\PTRSKEL.c's identical comment for why. */
      break;

   case STRATEGY_DEINSTALL:
      /* Restore whatever's under the cursor before this code/data becomes
       * invalid - see PTRSKEL.c's identical addition (Phase 3 cleanup). */
      EraseCursor();
      break;

   case STRATEGY_GENIOCTL:
      tcom('G');
      tcom_h8(prp->ioctl.bCategory);
      tcom(':');
      tcom_h8(prp->ioctl.bFunction);

      if (prp->ioctl.bCategory == IOCTL_SCR_AND_PTRDRAW &&
          prp->ioctl.bFunction == PTR_GETPTRDRAWADDRESS) {
         /* RUN 1 found the first caller gets a full 10-byte buffer (we
          * answer OK), but later callers send something shorter (we reject
          * with RPERR_LENGTH) - log the EXACT usDataLen/usParmLen this time
          * instead of just short-or-not, to find out what size(s) really
          * show up before deciding how to handle them. */
         tcom('L'); tcom_h16(prp->ioctl.usDataLen);
         tcom('P'); tcom_h16(prp->ioctl.usParmLen);
         if (prp->ioctl.usDataLen < sizeof(PTRDRAWFUNCTION)) {
            /* RUN 4 tried treating usDataLen==0 as a probe to answer with
             * fake success instead of rejecting - REVERTED: it crashed
             * VBOXMOUS (VirtualBox's own mouse-integration driver, not our
             * module), which apparently takes success to mean it received
             * a REAL draw-routine address and tried to use it, even though
             * we wrote nothing into the zero-length buffer. Runs 1-3's
             * outright rejection (RPDONE|RPERR|RPERR_LENGTH) was correct
             * and necessary, not a limitation - reporting fake success on
             * an empty buffer is actively dangerous. See PLAN.md Sec 4a for
             * the full finding: VBOXMOUS's probe failing is very likely
             * WHY the mouse pointer still looks fine even though our own
             * PtrDrawStub never does anything - VBox's own mouse-
             * integration mechanism, not real POINTDD delegation, is what's
             * actually drawing the cursor in this environment. */
            prp->usStatus = RPDONE | RPERR | RPERR_LENGTH;
            tcom('s');
         } else {
            PTRDRAWFUNCTION FAR16DATA *pResp =
               (PTRDRAWFUNCTION FAR16DATA *)prp->ioctl.pvData;
            pResp->usReturnCode = 0;
            pResp->pfnDraw = (PFN)PtrDrawStub;
            pResp->pchDataSeg = (char FAR16DATA *)0;
            tcom('R');
         }
      } else {
         prp->usStatus = RPDONE | RPERR | RPERR_BADCOMMAND;
         tcom('x');
      }
      break;

   default:
      prp->usStatus = RPDONE | RPERR | RPERR_GENERAL;
   }
}

#pragma code_seg ("_inittext");
#pragma data_seg ("_initdata","endds");

char cDevName[9] = "POINTER$";   /* THE REAL NAME - see file header */

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

      /* Phase 4: same mapping PTRSKEL.c uses - see its StrategyInit comment
       * for why this is done once here, not per-draw. */
      g_vgaText = (unsigned char far *) MapPhysToVirt(0xB8000UL, 4000UL);

      cprintf("POINTDD clone (SWAP-IN TEST, Phase 4): %s loaded OK.\n", cDevName);
   } else {
      prp->usStatus = RPDONE | RPERR;
      prp->init_out.usCodeEnd = 0;
      prp->init_out.usDataEnd = 0;
      cprintf("POINTDD clone (SWAP-IN TEST, Phase 4): %s init FAILED.\n", cDevName);
   }
}
