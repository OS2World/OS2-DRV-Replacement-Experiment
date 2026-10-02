/**
 * ctrytest.c - ring-3 test app to cross-check our byte-level COUNTRY.SYS
 * reverse-engineering (see ..\PLAN.md Sec 5) against what DosQueryCtryInfo
 * actually reports at runtime, for a given (country, codepage) pair.
 *
 * Ordinary 32-bit OS/2 console app, NOT a driver - no boot risk at all,
 * unlike the sibling POINTDD.SYS/KBDBASE.SYS projects. Build with wcl386,
 * same recipe as those projects' own test apps.
 *
 * Usage: ctrytest [country [codepage]]
 * Defaults to country=1, codepage=850 - exactly the (country, codepage) pair
 * already decoded by hand from the file's bytes (PLAN.md Sec 5c/5d), so the
 * output here can be compared field-by-field against that analysis directly.
 *
 * Unlike the driver-side source in the sibling projects, this is an ordinary
 * ring-3 application - no conflict between a driver-context header (like
 * Dev16lib.h) and the real Toolkit os2.h, so this just uses the official
 * COUNTRYCODE/COUNTRYINFO structs and DosQueryCtryInfo prototype directly
 * (INCL_DOSNLS), rather than hand-declaring anything locally.
 */
#define INCL_DOSNLS
#define INCL_DOSERRORS
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dump2(const char *label, CHAR *s)
{
   printf("  %-24s = \"%s\"  (bytes: %02x %02x)\n",
          label, s, (unsigned char)s[0], (unsigned char)s[1]);
}

int main(int argc, char *argv[])
{
   COUNTRYCODE cc;
   COUNTRYINFO ci;
   ULONG       cbActual;
   APIRET      rc;

   memset(&cc, 0, sizeof(cc));
   cc.country  = (argc > 1) ? (ULONG) atol(argv[1]) : 1;
   cc.codepage = (argc > 2) ? (ULONG) atol(argv[2]) : 850;

   printf("Querying country=%lu codepage=%lu ...\n", cc.country, cc.codepage);

   rc = DosQueryCtryInfo(sizeof(ci), &cc, &ci, &cbActual);
   if (rc) {
      printf("DosQueryCtryInfo FAILED, rc=%lu\n", rc);
      return 1;
   }

   printf("DosQueryCtryInfo OK - cbActual=%lu, sizeof(COUNTRYINFO)=%u\n\n",
          cbActual, (unsigned)sizeof(ci));

   printf("  country                  = %lu\n", ci.country);
   printf("  codepage                 = %lu\n", ci.codepage);
   printf("  fsDateFmt                = %u\n", ci.fsDateFmt);
   printf("  szCurrency               = \"%s\"  (bytes: %02x %02x %02x %02x %02x)\n",
          ci.szCurrency,
          (unsigned char)ci.szCurrency[0], (unsigned char)ci.szCurrency[1],
          (unsigned char)ci.szCurrency[2], (unsigned char)ci.szCurrency[3],
          (unsigned char)ci.szCurrency[4]);
   dump2("szThousandsSeparator", ci.szThousandsSeparator);
   dump2("szDecimal",            ci.szDecimal);
   dump2("szDateSeparator",      ci.szDateSeparator);
   dump2("szTimeSeparator",      ci.szTimeSeparator);
   printf("  fsCurrencyFmt            = %u\n", ci.fsCurrencyFmt);
   printf("  cDecimalPlace            = %u\n", ci.cDecimalPlace);
   printf("  fsTimeFmt                = %u\n", ci.fsTimeFmt);
   dump2("szDataSeparator",      ci.szDataSeparator);

   return 0;
}
