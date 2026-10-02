/**
 * ctrytest2.c - cross-checks the UNPROVEN part of the generated COUNTRY.SYS:
 * the type-dispatch table that's supposed to let DosQueryCollate/DosMapCase/
 * DosQueryDBCSEnv find the COLLATE/UCASE/DBCS tables for a given country.
 * Unlike ctrytest.c's DosQueryCtryInfo check (proven working end-to-end,
 * see ../PLAN.md Sec 6), this path's exact real-file framing was never
 * fully reverse-engineered (Sec 8) - ctybuild.c uses its own clean,
 * self-consistent design for this part instead of a byte-replica of IBM's
 * undiscovered one. This program is how we find out if that design works.
 *
 * Ordinary 32-bit OS/2 console app, NOT a driver - no boot risk. Usage:
 * ctrytest2 [country [codepage]], defaults to 1,850 to match ctrytest.c.
 */
#define INCL_DOSNLS
#define INCL_DOSERRORS
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
   COUNTRYCODE cc;
   APIRET rc;
   ULONG cbActual;
   unsigned char collateBuf[256];
   unsigned char dbcsBuf[32];
   unsigned char mapBuf[16] = "AbCdEfGhIjKlMnO";
   int i;

   memset(&cc, 0, sizeof(cc));
   cc.country  = (argc > 1) ? (ULONG) atol(argv[1]) : 1;
   cc.codepage = (argc > 2) ? (ULONG) atol(argv[2]) : 850;

   printf("Querying country=%lu codepage=%lu\n\n", cc.country, cc.codepage);

   /* --- DosQueryCollate --- */
   rc = DosQueryCollate(sizeof(collateBuf), &cc, collateBuf, &cbActual);
   if (rc) {
      printf("DosQueryCollate FAILED, rc=%lu\n", rc);
   } else {
      printf("DosQueryCollate OK, cbActual=%lu\n", cbActual);
      printf("  first 16 bytes: ");
      for (i = 0; i < 16; i++) printf("%02x ", collateBuf[i]);
      printf("\n  byte[0x41]('A')=%02x byte[0x61]('a')=%02x (expect same sort-equivalent if a==A)\n",
             collateBuf[0x41], collateBuf[0x61]);
   }
   printf("\n");

   /* --- DosMapCase (in-place case mapping) --- */
   printf("DosMapCase: before = \"%s\"\n", mapBuf);
   rc = DosMapCase(strlen((char*)mapBuf), &cc, (PCHAR)mapBuf);
   if (rc) {
      printf("DosMapCase FAILED, rc=%lu\n", rc);
   } else {
      printf("DosMapCase OK, after = \"%s\"\n", mapBuf);
   }
   printf("\n");

   /* --- DosMapCase on i/I and high-half bytes whose case pairs differ per
    * code page: 0x69 'i', 0x49 'I', 0x82 (e-acute), 0x8D (dotless i in CP857),
    * 0x98 (I with dot in CP857), 0xA0 (a-acute). No expectation is hardcoded:
    * run once with the real COUNTRY.SYS and once with the generated one and
    * compare the two outputs (they must be identical). */
   {
      static const unsigned char in[] = { 0x69, 0x49, 0x82, 0x8D, 0x98, 0xA0 };
      unsigned char hb[sizeof(in)];
      memcpy(hb, in, sizeof(in));
      rc = DosMapCase(sizeof(in), &cc, (PCHAR)hb);
      if (rc) {
         printf("DosMapCase(high bytes) FAILED, rc=%lu\n", rc);
      } else {
         printf("DosMapCase high bytes:  in =");
         for (i = 0; i < (int)sizeof(in); i++) printf(" %02x", in[i]);
         printf("\n                       out =");
         for (i = 0; i < (int)sizeof(in); i++) printf(" %02x", hb[i]);
         printf("\n");
      }
      printf("\n");
   }

   /* --- DosQueryDBCSEnv --- */
   rc = DosQueryDBCSEnv(sizeof(dbcsBuf), &cc, (PCHAR)dbcsBuf);
   if (rc) {
      printf("DosQueryDBCSEnv FAILED, rc=%lu\n", rc);
   } else {
      printf("DosQueryDBCSEnv OK\n  lead-byte ranges: ");
      for (i = 0; i < 16; i += 2) {
         if (dbcsBuf[i] == 0 && dbcsBuf[i+1] == 0) { printf("(end)"); break; }
         printf("[%02x-%02x] ", dbcsBuf[i], dbcsBuf[i+1]);
      }
      printf("\n");
   }

   return 0;
}
