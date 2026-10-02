/**
 * KBDBASE.c - open-source clone of OS/2's hardware-independent keyboard
 * device driver (KBDBASE.SYS) - OpenWatcom + Drv16Kit port.
 *
 * Named to match the KBDBASE.sys target (wmake's .c.obj/.obj.sys inference
 * rules chain by matching base filename - same lesson learned on the
 * sibling POINTDD.SYS project, see ..\..\POINTDD\PLAN.md).
 *
 * *** READ ..\PLAN.md BEFORE TOUCHING THIS FILE. *** Two things matter more
 * here than on any prior project in this series:
 *
 * 1. CLEAN-ROOM DISCIPLINE. Unlike POINTDD.SYS, actual IBM source for this
 *    driver exists locally (both DDK archives), marked IBM Confidential /
 *    Restricted Materials of IBM. That source - and the existing toolkit
 *    note derived from reading it (os2ref\vio-kbd-mou.md) - must NEVER be
 *    consulted while writing this file, for any reason. Everything here
 *    comes from PUBLIC documentation only: os2tk45\h\bsedev.h (the
 *    IOCTL_KEYBOARD category and KBD_* function constants - a public
 *    Toolkit header, not DDK-internal source) and os2ddk2004\Docs-PDF\
 *    PDDREF.pdf's "Category 04h Keyboard Control IOCtl Commands" section
 *    (the Physical Device Driver Reference manual, explicitly written for
 *    third-party driver developers - the same document category
 *    POINTDD.SYS's own protocol came from). If public docs run out before
 *    a question is answered, that's a STOP-AND-ASK moment, not a reason to
 *    peek at the confidential source.
 *
 * 2. NO SAFE DECOY-NAME PHASE. KBDBASE.SYS "cannot be renamed" (IBM's own
 *    DDK docs, os2ddk2004\HTML\REF\USEDDK\00111.HTM) - the kernel
 *    hard-loads this exact filename at system init, with no CONFIG.SYS
 *    line to redirect it (confirmed on the test VM: C:\OS2\BOOT\
 *    KBDBASE.SYS). Every boot test of this file IS the real thing from day
 *    one - there is no additive/PTRSKEL$-style warm-up phase like POINTDD
 *    had. User has explicitly accepted this risk for a disposable test VM;
 *    standing procedure is a fresh VirtualBox snapshot before every single
 *    boot test (PLAN.md Sec 4).
 *
 * Phase 1 (this file, starting 2026-10-01): prove the toolchain produces a
 * driver the kernel loads and runs Init/InitComplete on without hanging or
 * trapping the boot - same first goal as POINTDD.SYS's Phase 1, adapted
 * for a kernel-hardcoded BASEDEV-equivalent load instead of an ordinary
 * DEVICE= line. Deliberately does NOT attempt real keyboard functionality
 * yet: the IDC handshake IBMKBD.SYS (the hardware-dependent sibling, which
 * stays the real stock driver throughout - see PLAN.md Sec 3) needs to
 * attach to us is architecture-internal wiring, NOT part of the public
 * Category 04h ring-3 IOCtl contract - PDDREF.pdf was checked directly and
 * confirms this gap (PLAN.md Sec 3). Realistic expected outcome: boots
 * without hanging/trapping, but keyboard input very likely does NOT work
 * at all post-boot, since IBMKBD.SYS has no documented protocol to honor
 * yet. That is expected, informative data for this phase, not a failure -
 * same incremental-empirical approach POINTDD.SYS used throughout.
 *
 * Every STRATEGY_GENIOCTL this phase traces the category/function it
 * received (COM1, same tcom() technique proven on POINTDD.SYS) and safely
 * rejects it (RPERR_BADCOMMAND) - we observe what actually arrives before
 * implementing anything real, rather than guess at Category 04h's finer
 * points ahead of real data.
 */
#include "Dev16lib.h"

/* Device name this driver registers under. INFERRED, not explicitly
 * confirmed: PDDREF.pdf's intro section (character device driver overview)
 * lists example device names "SCREENS, KBD$, LPT1 or COM2" - one
 * representative example per common device type (display, keyboard,
 * parallel port, serial port) - strongly implying KBD$ is the real
 * keyboard device name, but the document never states this as an explicit
 * fact tied to KBDBASE.SYS specifically. See PLAN.md Sec 3. Whether this
 * matters for Phase 1 at all is itself an open question - the kernel loads
 * this file by its FILENAME, not by this registered name, so Strategy_Init
 * succeeding doesn't depend on getting this exactly right; it only matters
 * for whichever ring-3 callers or IBMKBD.SYS's IDC attach try to find us
 * by name afterward, which this phase observes empirically rather than
 * assumes. */
char cDevName[5] = "KBD$";

/* IOCTL_KEYBOARD is the public Category 04h constant (os2tk45\h\bsedev.h) -
 * not yet confirmed whether strategy.h (sourced from the external DDK at
 * build time, see PLAN.md Sec 5) already defines this the way it already
 * defined IOCTL_SCR_AND_PTRDRAW for the POINTDD project. Defined locally
 * either way, matching this project's established practice of not
 * depending on bsedev.h directly (that header assumes the full ring-3
 * os2.h umbrella, which doesn't mix cleanly with Dev16lib.h's own
 * driver-context types - same reasoning as POINTDD.SYS's PTRDRAWFUNCTION). */
#ifndef IOCTL_KEYBOARD
#define IOCTL_KEYBOARD 0x0004
#endif

/* Resolves the forward reference from StrategyHandler below. */
void StrategyInit(PREQPACKET prp);

/* Raw COM1 (0x3F8) trace, built on Dev16lib.h's PortInByte/PortOutByte -
 * proven safe at any time post-init throughout the POINTDD.SYS project
 * (unlike iprintf/cprintf's underlying DevHelp_Save_Message, which turned
 * out to be boot-time-only despite documentation that read otherwise on a
 * first pass - see that project's PLAN.md for the full trap-and-fix
 * writeup). Must stay in the permanent code segment, like everything
 * STRATEGY_GENIOCTL needs post-init. */
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

/* DAW_IDC: bit 14 (0x4000) of the device attribute word, "set if driver
 * supports inter-device communications (IDC)". This is GENERIC, PUBLIC OS/2
 * PDD architecture - the same device-header layout (link/attribute/strategy-
 * offset/IDC-offset/name) and attribute-word bit table apply to every OS/2
 * character device driver, not anything specific to KBDBASE.SYS's own
 * implementation. Source: Mastrianni, "Writing OS/2 Device Drivers in C"
 * (3rd ed., 1997), Table 5-1 "Device Attribute Word" and the AttachDD
 * DevHelp reference ("the DAW_IDC bit in the device attribute word must
 * also be set, otherwise the AttachDD call from the other device driver
 * will fail... before the device driver calls the entry point, it must
 * verify that the entry point received is nonzero... must follow the FAR
 * CALL/RET model") - a published, commercially available programming book,
 * not IBM DDK source. Drv16Kit's own Dev16lib.h (David Azarewicz's kit, not
 * IBM's) independently confirms the same struct shape - DEV_HEADER has
 * usAttribs and pfnIDC fields, and both are exposed via the plain extern
 * global `Header` - but the kit documents no setter for either and ships
 * its device-header template only as a precompiled Drv16.lib(header)
 * object, not editable source, so its DEFAULT values for these two fields
 * are unknown/opaque. */
#ifndef DAW_IDC
#define DAW_IDC 0x4000
#endif

/* Safe IDC entry-point stub. Working theory for the Trap 000D "Exception in
 * module: IBMKBD" seen on the previous boot test (PLAN.md "Third boot
 * test"): with DAW_IDC unset and Header.pfnIDC left at whatever
 * Drv16.lib(header)'s opaque default is (likely nonzero garbage, not a
 * clean zero), IBMKBD.SYS's own AttachDD-based nonzero-pointer check (see
 * the Mastrianni comment above) may pass on that garbage value and then
 * jump to it, crashing on IBMKBD's own side rather than ours - consistent
 * with our StrategyInit having already completed successfully (traced 'IY')
 * before the crash, and no further Strategy/GenIOCtl commands ever reaching
 * our StrategyHandler.
 *
 * This stub does NOT implement the real KBDBASE<->IBMKBD IDC protocol -
 * that protocol's actual function numbers/packet formats are not in any
 * public document found so far (PLAN.md Sec 3) and deliberately not sought
 * from the confidential source either. It only follows the generic,
 * publicly-documented contract above (a real, valid, far-call/ret routine)
 * well enough that calling it should be safe rather than catastrophic -
 * touches no registers, does nothing, returns immediately. Traced via COM1
 * so we can at least observe IF/when it gets called. Must stay resident
 * (declared here, before the _inittext/_initdata pragma switch below) -
 * IBMKBD.SYS could call this at any time post-boot, not just during init. */
void far KbdIdcStub(void)
{
   tcom('D'); tcom('C');   /* 'DC' = (I)DC entry point was reached */
}

/* The kernel calls this whenever it wants to access the driver - must be
 * declared exactly this way (far, ES:BX parm) per Drv16Kit's own doc, same
 * as POINTDD.SYS's StrategyHandler. */
#pragma aux StrategyHandler far parm [es bx];
void StrategyHandler(PREQPACKET prp)
{
   /* Default status unless changed below - must always have RPDONE set. */
   prp->usStatus = RPDONE;

   switch (prp->bCommand) {

   case STRATEGY_INIT:
      /* Called at ring 3 (actually: by the kernel loader), immediately
       * after load. */
      StrategyInit(prp);
      break;

   case 0x1B:
      /* EMPIRICALLY OBSERVED (com.log from this project's own boot test,
       * 2026-10-01): the kernel sends Strategy command 0x1B (27 decimal),
       * NOT the standard STRATEGY_INIT (0) that ordinary CONFIG.SYS-loaded
       * drivers like POINTDD.SYS receive, to invoke Init for this
       * kernel-initialization-time-loaded driver. Before this case existed,
       * command 0x1B fell through to the default: branch below, got traced
       * as 'S1B', and returned RPERR_GENERAL - StrategyInit never ran at
       * all, which is why the boot failed with "device driver KBDBASE.SYS
       * ... failed to install" rather than the earlier "cannot find the
       * file" (that part was already fixed - see StrategyInit's own
       * comment on the cprintf/MSG-import fix). This constant is not named
       * STRATEGY_* because it isn't in strategy.h and its real meaning
       * isn't confirmed beyond "this is what gets Init to run here" -
       * observed behavior, not documented protocol. */
      StrategyInit(prp);
      break;

   case STRATEGY_INITCOMPLETE:
      /* Called after every driver in the system has loaded. Phase 1 does
       * nothing here beyond the kit's own required call - same as
       * POINTDD.SYS's Phase 1. */
      Drv16InitComplete();
      break;

   case STRATEGY_OPEN:
   case STRATEGY_CLOSE:
   case STRATEGY_SAVERESTORE:
      /* Not needed yet - default RPDONE above already covers these. */
      break;

   case STRATEGY_GENIOCTL:
      /* Phase 1: trace every GenIOCtl that arrives (category/function) via
       * COM1, then safely reject it - we observe real traffic before
       * implementing anything, same discipline as POINTDD.SYS's Phase 2.
       * 'G' cat:func */
      tcom('G');
      tcom_h8(prp->ioctl.bCategory);
      tcom(':');
      tcom_h8(prp->ioctl.bFunction);
      prp->usStatus = RPDONE | RPERR | RPERR_BADCOMMAND;
      tcom('x');
      break;

   default:
      /* Trace unhandled Strategy commands too (not just GenIOCtl) - given
       * this driver's central role, seeing what the kernel/IBMKBD.SYS
       * actually sends beyond the commands above is itself useful Phase 1
       * data. 'S' cmd (the Strategy bCommand byte, not an IOCtl). */
      tcom('S');
      tcom_h8(prp->bCommand);
      prp->usStatus = RPDONE | RPERR | RPERR_GENERAL;
   }
}

/* Discardable after init - everything below goes into the init-only
 * text/data segments the kernel throws away once loading finishes. */
#pragma code_seg ("_inittext");
#pragma data_seg ("_initdata","endds");

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

      /* Advertise IDC support and point at a real, safe entry point instead
       * of whatever Drv16.lib(header)'s opaque default leaves Header.pfnIDC
       * as - see KbdIdcStub's comment above for the full rationale and the
       * generic (non-KBDBASE-specific) public source for DAW_IDC/pfnIDC. */
      Header.usAttribs |= DAW_IDC;
      Header.pfnIDC = (USHORT) KbdIdcStub;

      /* NOT cprintf() here - see file header. cprintf wraps DevHelp_Save_
       * Message, which pulls in an import from the MSG module (confirmed via
       * KBDBASE.map: DOS16PUTMESSAGE/MSG) - the real stock KBDBASE.SYS only
       * imports from DOSCALLS (EDM2: "DLLs Loaded: DOSCALL1.DLL"), no MSG
       * import at all. For a kernel-initialization-time driver loaded before
       * ordinary DEVICE= drivers, that extra import may not be resolvable
       * yet, and was the leading suspect for the "cannot find the file" boot
       * failure (PLAN.md "NE header comparison"). COM1 trace only - no
       * external module dependency at all. */
      tcom('I'); tcom('Y');
   } else {
      prp->usStatus = RPDONE | RPERR;
      prp->init_out.usCodeEnd = 0;
      prp->init_out.usDataEnd = 0;
      tcom('I'); tcom('N');
   }
}
