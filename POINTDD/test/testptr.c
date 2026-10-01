/**
 * testptr.c - ring-3 test app for PTRSKEL$'s PTR_GETPTRDRAWADDRESS handler.
 *
 * Why this exists: PTRSKEL$ is a decoy device name (see ..\PLAN.md Sec 4) -
 * nothing in a real OS/2 system knows to query an unknown device for
 * pointer-draw capability, so simply booting the driver never exercises its
 * new Phase 2 GenIOCtl code at all. This app calls DosDevIOCtl directly
 * against our own driver, under our own control, to verify that code path
 * actually works - the same pattern the Drv16Kit's own Sample\test.c uses to
 * test its sample driver's IOCtl interface.
 *
 * Ordinary 32-bit OS/2 console app (NOT a driver) - build with OpenWatcom's
 * wcl386, per ..\..\..\CLAUDE.md's "Build a console (VIO) app" recipe.
 *
 * Phase 3 addition: also calls PTR_TEST_INVOKE_DRAW (0xF0, our own private
 * test-trigger function, not real protocol) twice, to prove PtrDrawStub
 * genuinely executes and receives correct parameters - see PTRSKEL.c's
 * PTR_TEST_INVOKE_DRAW comment for why this indirect trigger, rather than
 * calling the returned pfnDraw address directly from this 32-bit app.
 *
 * Phase 3 step 3: the driver now alternates between two fixed test cells
 * (same row, different column) on each PTR_TEST_INVOKE_DRAW call, exercising
 * draw -> move -> erase-and-restore. This app checks that the second call's
 * "restored old cell" exactly matches the first call's "save-under" values -
 * i.e. that moving the cursor away puts back EXACTLY what was there before,
 * byte for byte. Must run from a genuine full-screen VIO/text session (a
 * PM-windowed command prompt doesn't map to live 0xB8000 - see PLAN.md
 * Sec 4b).
 */
#define INCL_NOPMAPI
#define INCL_DOSDEVIOCTL
#define INCL_BASE
#include <os2.h>
#include <stdio.h>
#include <string.h>

/* IOCTL_SCR_AND_PTRDRAW and PTR_GETPTRDRAWADDRESS are already defined by
 * os2.h itself here (INCL_DOSDEVIOCTL pulls in the real bsedev.h, since this
 * is a ring-3 app - unlike PTRSKEL.c, which can't use bsedev.h directly, see
 * that file's own comment on why). Must match its values: 0x0003 / 0x72. */

#define OPEN_FLAG ( OPEN_ACTION_OPEN_IF_EXISTS )
#define OPEN_MODE ( OPEN_FLAGS_FAIL_ON_ERROR | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE )

/* Byte-for-byte match of PTRSKEL.c's local PTRDRAWFUNCTION (pack 2):
 * USHORT usReturnCode + a 16:16 far pointer (4 bytes) + another 16:16 far
 * pointer (4 bytes) = 10 bytes, no padding. DosDevIOCtl only copies raw
 * bytes across the ring-3/ring-0 boundary - we don't dereference these as
 * real 32-bit pointers, just inspect the raw selector:offset values. */
#pragma pack(2)
typedef struct {
   USHORT usReturnCode;
   ULONG  pfnDraw;      /* 16:16 far ptr, low word=offset, high word=selector */
   ULONG  pchDataSeg;   /* same */
} PTRDRAWRESP;
#pragma pack()

/* Phase 3: our own private test-trigger function - not in os2.h, since it's
 * not a real documented function (see PTRSKEL.c's PTR_TEST_INVOKE_DRAW
 * comment for the full why). Must match its value exactly: 0xF0. */
#define PTR_TEST_INVOKE_DRAW   0xF0

/* Byte-for-byte match of PTRSKEL.c's local PTRTESTRESULT (pack 2). */
#pragma pack(2)
typedef struct {
   USHORT usCallCount;
   LONG   lastX;
   LONG   lastY;
   UCHAR  curChar;
   UCHAR  curAttr;
   UCHAR  savedChar;
   UCHAR  savedAttr;
   UCHAR  oldCellChar;
   UCHAR  oldCellAttr;
   USHORT vgaPtrOff;
   USHORT vgaPtrSeg;
} PTRTESTRESULT;
#pragma pack()

int main(void)
{
   HFILE  hfile;
   ULONG  ulAction;
   APIRET rc;
   PTRDRAWRESP resp;
   ULONG  dataLenOut;

   rc = DosOpen((PSZ)"PTRSKEL$", &hfile, &ulAction, 0, 0,
                OPEN_FLAG, OPEN_MODE, NULL);
   if (rc) {
      printf("DosOpen(PTRSKEL$) failed, rc=%lu\n", rc);
      return 1;
   }
   printf("DosOpen(PTRSKEL$) OK, handle=%lu\n", (ULONG)hfile);

   memset(&resp, 0xCC, sizeof(resp));   /* poison, so unset fields are obvious */
   dataLenOut = sizeof(resp);

   rc = DosDevIOCtl(hfile, IOCTL_SCR_AND_PTRDRAW, PTR_GETPTRDRAWADDRESS,
                    NULL, 0, NULL,
                    (PVOID)&resp, sizeof(resp), &dataLenOut);

   if (rc) {
      printf("DosDevIOCtl(cat=3,func=0x72) FAILED, rc=%lu (0x%lx)\n", rc, rc);
   } else {
      printf("DosDevIOCtl(cat=3,func=0x72) OK, dataLenOut=%lu\n", dataLenOut);
      printf("  usReturnCode = 0x%04x\n", resp.usReturnCode);
      printf("  pfnDraw      = %04lx:%04lx (sel:off)\n",
             (resp.pfnDraw >> 16) & 0xFFFF, resp.pfnDraw & 0xFFFF);
      printf("  pchDataSeg   = %04lx:%04lx (sel:off)\n",
             (resp.pchDataSeg >> 16) & 0xFFFF, resp.pchDataSeg & 0xFFFF);
   }

   /* Phase 3 step 3: call PTR_TEST_INVOKE_DRAW three times. The driver
    * cycles through three fixed test cells - move 0->1 is same-row
    * (column only), move 1->2 changes row AND column (a genuine multi-row
    * move) - so this exercises draw -> move -> erase-and-restore in both
    * directions. Each call's "savedChar/savedAttr" (what was under the
    * cursor before it was drawn) must exactly match the NEXT call's
    * "oldCellChar/oldCellAttr" (what's at that same cell after the cursor
    * moved away and it was restored) - that's the save-under round-trip
    * actually being correct, not just "no crash", now verified across a
    * row change too. */
   {
      int i;
      UCHAR prevSavedChar = 0, prevSavedAttr = 0;

      for (i = 0; i < 3; i++) {
         PTRTESTRESULT tr;
         ULONG trLenOut = sizeof(tr);
         memset(&tr, 0xCC, sizeof(tr));

         rc = DosDevIOCtl(hfile, IOCTL_SCR_AND_PTRDRAW, PTR_TEST_INVOKE_DRAW,
                          NULL, 0, NULL,
                          (PVOID)&tr, sizeof(tr), &trLenOut);
         if (rc) {
            printf("DosDevIOCtl(cat=3,func=0xF0) call %d FAILED, rc=%lu\n", i, rc);
            continue;
         }

         printf("DosDevIOCtl(cat=3,func=0xF0) call %d OK: callCount=%u lastX=%ld lastY=%ld\n",
                i, tr.usCallCount, tr.lastX, tr.lastY);
         printf("  drawn cell:  char=0x%02x attr=0x%02x (saved under: char=0x%02x attr=0x%02x)\n",
                tr.curChar, tr.curAttr, tr.savedChar, tr.savedAttr);
         printf("  old cell after restore: char=0x%02x attr=0x%02x\n",
                tr.oldCellChar, tr.oldCellAttr);

         if (i >= 1) {
            if (tr.oldCellChar == prevSavedChar && tr.oldCellAttr == prevSavedAttr) {
               printf("  SAVE-UNDER OK: restored cell matches call %d's save-under exactly.\n",
                      i - 1);
            } else {
               printf("  SAVE-UNDER MISMATCH: expected char=0x%02x attr=0x%02x\n",
                      prevSavedChar, prevSavedAttr);
            }
         }
         prevSavedChar = tr.savedChar;
         prevSavedAttr = tr.savedAttr;
      }
   }

   rc = DosClose(hfile);
   if (rc) {
      printf("DosClose failed, rc=%lu\n", rc);
      return 1;
   }

   return 0;
}
