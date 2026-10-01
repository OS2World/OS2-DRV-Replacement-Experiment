/*
 * strat.c - POINTDD clone: Strategy + Init + InitComplete (Phase 1)
 *
 * PHASE 1 GOAL: prove the 16-bit DDK toolchain (MS C 6.0 + MASM) produces a
 * driver that the kernel loads and runs Init/InitComplete on WITHOUT hanging
 * or trapping the boot - nothing else. No hardware access, no AttachDD, no
 * IOCtl handling. Registers as PTRSKEL$ (a decoy name - see ptr_hdr.asm), so
 * it is purely additive alongside the real, untouched POINTDD.SYS/MOUSE.SYS.
 *
 * Modeled on the WiFi RTL8188EU driver's strat.c (same toolchain, same
 * Strategy/Init/InitComplete dispatch pattern, same COM1 tcom() trace
 * technique for debugging a driver with no console). Two hard-won facts
 * carried over from that project, both load-bearing here too:
 *   1. Strategy's ES:BX->request-packet capture MUST run with optimization
 *      off, or the compiler's prologue clobbers ES:BX before the inline asm
 *      reads them (garbage pRP -> Init reads bogus DevHlpEP -> boot hang).
 *   2. tcom() must never truly block (bounded wait only) so a debug trace
 *      can never itself be the thing that hangs the boot.
 */
#include <os2.h>
#include <devhdr.h>
#include <devcmd.h>
#include <strat2.h>        /* defines P_DriverCaps, needed by reqpkt.h        */
#include <reqpkt.h>
#include <dhcalls.h>

/* ---- driver globals ---------------------------------------------------- */
PFN Device_Help = 0;             /* DevHelp entry point (saved from CMDInit) */

/* Forward declarations: Init() (below) takes the address of these resident-
 * boundary markers, but they're only DEFINED at the very end of this file
 * (deliberately - see the comment down there for why). Unlike the WiFi
 * driver, where the markers lived in a different .c file and were declared
 * via a shared header, everything here is one file, so they need an
 * explicit forward declaration before first use or the compiler has no idea
 * what they are yet. */
extern void ResidentEnd(void);
extern char g_ResidentDataEnd;

/* ---- boot-trace: write one raw byte to the COM1 UART (0x3F8) ----------
 * Same technique as the WiFi driver: bypasses any console/dprintf gating,
 * VirtualBox's raw-file COM1 capture records every byte, and the LAST byte
 * seen on a hang tells us exactly how far Init/Strategy got. */
#pragma optimize("", off)
void tcom(char c)
{
   _asm {
      mov  cx, 0FFFFh          ; bounded wait so we never hang here
      mov  dx, 03FDh           ; LSR (line status)
   twait:
      in   al, dx
      test al, 20h             ; bit5 = transmit holding reg empty?
      jnz  tsend
      loop twait
   tsend:
      mov  dx, 03F8h           ; THR (transmit)
      mov  al, byte ptr c
      out  dx, al
   }
}
#pragma optimize("", on)

/* ---- Init: CMDInit ------------------------------------------------------
 * Saves DevHelp, reports the resident CodeEnd/DataEnd markers, and stops -
 * no AttachDD, no hardware, nothing that can fail. */
static void Init(PRPH pRP)
{
   PRPINITIN  pRPI = (PRPINITIN)pRP;
   PRPINITOUT pRPO = (PRPINITOUT)pRP;

   tcom('I');                          /* Init entered                       */
   Device_Help = pRPI->DevHlpEP;

   pRPO->Unit     = 0;
   pRPO->CodeEnd  = (USHORT)&ResidentEnd;
   pRPO->DataEnd  = (USHORT)&g_ResidentDataEnd;
   pRPO->BPBArray = 0;

   pRPO->rph.Status |= STDON;
   tcom('K');                          /* Init done                          */
}

/* ---- InitComplete: CMDInitComplete -------------------------------------- */
static void InitComplete(PRPH pRP)
{
   tcom('C');                          /* InitComplete entered                */
   pRP->Status |= STDON;
   tcom('K');                          /* InitComplete done                   */
}

/* ---- Strategy: the driver's request entry point -------------------------
 * ES:BX -> Request Packet. Optimization OFF (see file header note 1). */
#pragma optimize("eglt", off)
void far Strategy(void)
{
   PRPH pRP;

   _asm {
      mov  word ptr pRP[0], bx
      mov  word ptr pRP[2], es
   }

   tcom('S');                          /* Strategy entered, ES:BX captured    */
   pRP->Status = 0;

   switch (pRP->Cmd) {
   case CMDInit:          Init(pRP);          break;
   case CMDInitComplete:  InitComplete(pRP);  break;

   default:
      /* Phase 1 handles nothing else - acknowledge as done (no error). This
       * is also why DEV_IOCTL/DEV_30 are NOT set in ptr_hdr.asm: we're not
       * asking the kernel to route Open/Close/IOCtl to us at all yet. */
      pRP->Status |= STDON;
      break;
   }
}
#pragma optimize("", on)

/* =======================================================================
 * Resident-boundary markers - MUST remain the very last code and data in
 * this file (and this file is the only/last object linked - see makefile).
 * Init() reports their offsets as CodeEnd/DataEnd so the kernel keeps the
 * whole (tiny) driver resident. Non-zero initializer forces g_ResidentDataEnd
 * into _DATA rather than _BSS (same reasoning as the WiFi driver's marker -
 * a _BSS placement could land before other data and under-report DataEnd).
 * ======================================================================= */
void ResidentEnd(void)
{
}
char g_ResidentDataEnd = 1;
