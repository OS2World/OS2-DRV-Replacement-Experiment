/**
 * ctybuild.c - generates a COUNTRY.SYS from the extracted real data in
 * cty_data.c (see ../PLAN.md Sec 8-9). Ordinary 32-bit OS/2 console app, not a
 * driver - no boot risk to build/run this; the only risk is whether OS/2
 * accepts the OUTPUT file (it structurally validates COUNTRY= files at boot
 * and halts with SYS02069 if it doesn't like one - see PLAN.md Sec 9).
 *
 * File layout (verified field-by-field against the real ArcaOS 5.1 file and
 * against Wim Brul's BSD-3-Clause cty593ec.inc, PLAN.md Sec 7/9):
 *
 *   header      23 bytes: FF "COUNTRY" 1,0,0,0,0,0,0,0, 1,0,1, <dir offset=23>, 0,0,0
 *   count       WORD: number of directory entries
 *   directory   count x 14 bytes: { 12, keyCountry, keyCodepage, 0, 0, table2Off, 0 }
 *               (304 keys in the real file; several alias another record's table2)
 *   table2      one per record (291), 50 bytes each: [ 6 ] then 6 x { 8, type, leafOff, 0 } with
 *               type order 1(CTYINFO) 6(COLLATE) 2(UCASE) 4(UCASE/FUCASE)
 *               5(FCHAR) 7(DBCS) - one self-contained table per directory entry
 *   CTYINFO     count x 48 bytes
 *   COLLATE     266 bytes each   (10-byte header + 256)
 *   UCASE/FUCASE 138 bytes each  (10-byte header + 128)
 *   FCHAR       32 bytes         (10-byte header + 22)
 *   DBCS        10 + reclen bytes each
 *
 * All offsets are absolute file offsets stored in 16-bit fields (that's what the
 * real format does - the real file is 50,947 bytes), so the whole file must
 * stay under 64 KB; the generator checks this and refuses to write otherwise.
 *
 * Usage: ctybuild [OUTFILE]      (default COUNTRY.NEW)
 */
#define INCL_NOPMAPI
#define INCL_BASE
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ctybuild.h"

#define HEADER_LEN       23
#define DIR_REC_LEN      14
#define TABLE2_LEN       50      /* 2-byte count + 6 x 8-byte entries */
#define CTYINFO_REC_LEN  48
#define COLLATE_REC_LEN  (10+256)
#define UCASE_REC_LEN    (10+128)
#define FCHAR_REC_LEN    (10+22)

static const unsigned char g_header[HEADER_LEN] = {
   0xff,'C','O','U','N','T','R','Y', 1,0,0,0,0,0,0,0,
   1,0,1,HEADER_LEN,0,0,0
};

static long g_dirBase, g_table2Base, g_ctyinfoBase;
static long g_collateBase, g_ucaseBase, g_fcharBase, g_dbcsBase, g_fileEnd;
static long *g_dbcsOffsets;

static void putU16(FILE *f, unsigned short v) { fputc(v & 0xFF, f); fputc((v>>8)&0xFF, f); }
static void putU8(FILE *f, unsigned char v) { fputc(v, f); }
static void putBytes(FILE *f, const unsigned char *p, int n) {
   int i; for (i = 0; i < n; i++) fputc(p[i], f);
}

int main(int argc, char *argv[])
{
   FILE *f;
   int i;
   long off;
   const char *outname = (argc > 1) ? argv[1] : "COUNTRY.NEW";

   g_dbcsOffsets = (long *) malloc(sizeof(long) * g_numDbcsTables);

   /* --- pass 1: section base offsets --- */
   g_dirBase     = HEADER_LEN + 2;                     /* after header + count word */
   g_table2Base  = g_dirBase + (long)g_numDirEntries * DIR_REC_LEN;
   g_ctyinfoBase = g_table2Base + (long)g_numCountries * TABLE2_LEN;
   g_collateBase = g_ctyinfoBase + (long)g_numCountries * CTYINFO_REC_LEN;
   g_ucaseBase   = g_collateBase + (long)g_numCollateTables * COLLATE_REC_LEN;
   g_fcharBase   = g_ucaseBase + (long)g_numUcaseTables * UCASE_REC_LEN;
   g_dbcsBase    = g_fcharBase + (long)g_numFcharTables * FCHAR_REC_LEN;
   off = g_dbcsBase;
   for (i = 0; i < g_numDbcsTables; i++) {
      g_dbcsOffsets[i] = off;
      off += 10 + g_dbcsTables[i].reclen;
   }
   g_fileEnd = off;

   printf("Layout:\n");
   printf("  header:   0x%06lx (%d bytes) + count word\n", 0L, HEADER_LEN);
   printf("  dir:      0x%06lx (%d x %d bytes)\n", g_dirBase, g_numDirEntries, DIR_REC_LEN);
   printf("  table2:   0x%06lx (%d x %d bytes)\n", g_table2Base, g_numCountries, TABLE2_LEN);
   printf("  ctyinfo:  0x%06lx (%d x %d bytes)\n", g_ctyinfoBase, g_numCountries, CTYINFO_REC_LEN);
   printf("  collate:  0x%06lx (%d x %d bytes)\n", g_collateBase, g_numCollateTables, COLLATE_REC_LEN);
   printf("  ucase:    0x%06lx (%d x %d bytes)\n", g_ucaseBase, g_numUcaseTables, UCASE_REC_LEN);
   printf("  fchar:    0x%06lx (%d x %d bytes)\n", g_fcharBase, g_numFcharTables, FCHAR_REC_LEN);
   printf("  dbcs:     0x%06lx (%d tables, variable)\n", g_dbcsBase, g_numDbcsTables);
   printf("  total:    %ld bytes\n", g_fileEnd);

   if (g_fileEnd > 0xFFFF) {
      printf("ERROR: file would be %ld bytes - offsets are 16-bit in this format\n", g_fileEnd);
      return 1;
   }

   f = fopen(outname, "wb");
   if (!f) { printf("Cannot create %s\n", outname); return 1; }

   /* --- header + directory count --- */
   putBytes(f, g_header, HEADER_LEN);
   putU16(f, (unsigned short) g_numDirEntries);

   /* --- directory: { 12, keyCountry, keyCodepage, 0, 0, table2Off, 0 } --- */
   for (i = 0; i < g_numDirEntries; i++) {
      const DIR_ENTRY *d = &g_dirEntries[i];
      putU16(f, 12);
      putU16(f, d->country);
      putU16(f, d->codepage);
      putU16(f, 0);
      putU16(f, 0);
      putU16(f, (unsigned short)(g_table2Base + (long)d->recordIdx * TABLE2_LEN));
      putU16(f, 0);
   }

   /* --- table2: [6] then 6 x { 8, type, leafOff, 0 } --- */
   for (i = 0; i < g_numCountries; i++) {
      const COUNTRY_RECORD *c = &g_countries[i];
      long ctyOff     = g_ctyinfoBase + (long)i * CTYINFO_REC_LEN;
      long collateOff = g_collateBase + (long)c->collateIdx * COLLATE_REC_LEN;
      long ucaseAOff  = g_ucaseBase   + (long)c->ucaseAIdx * UCASE_REC_LEN;
      long ucaseBOff  = g_ucaseBase   + (long)c->ucaseBIdx * UCASE_REC_LEN;
      long fcharOff   = g_fcharBase   + (long)c->fcharIdx * FCHAR_REC_LEN;
      long dbcsOff    = g_dbcsOffsets[c->dbcsIdx];

      putU16(f, 6);
      putU16(f, 8); putU16(f, 1); putU16(f, (unsigned short) ctyOff);     putU16(f, 0);
      putU16(f, 8); putU16(f, 6); putU16(f, (unsigned short) collateOff); putU16(f, 0);
      putU16(f, 8); putU16(f, 2); putU16(f, (unsigned short) ucaseAOff);  putU16(f, 0);
      putU16(f, 8); putU16(f, 4); putU16(f, (unsigned short) ucaseBOff);  putU16(f, 0);
      putU16(f, 8); putU16(f, 5); putU16(f, (unsigned short) fcharOff);   putU16(f, 0);
      putU16(f, 8); putU16(f, 7); putU16(f, (unsigned short) dbcsOff);    putU16(f, 0);
   }

   /* --- CTYINFO records --- */
   for (i = 0; i < g_numCountries; i++) {
      const COUNTRY_RECORD *c = &g_countries[i];
      putU8(f, 0xFF);
      putBytes(f, (const unsigned char *)"CTYINFO", 7);
      putU16(f, 38);
      putU16(f, c->country);
      putU16(f, c->codepage);
      putU16(f, c->fsDateFmt);
      putBytes(f, c->szCurrency, 5);
      putBytes(f, c->szThousandsSeparator, 2);
      putBytes(f, c->szDecimal, 2);
      putBytes(f, c->szDateSeparator, 2);
      putBytes(f, c->szTimeSeparator, 2);
      putU8(f, c->fsCurrencyFmt);
      putU8(f, c->cDecimalPlace);
      putU8(f, c->fsTimeFmt);
      putU16(f, 0); putU16(f, 0);                                  /* reserved1 */
      putBytes(f, c->szDataSeparator, 2);
      putU16(f,0); putU16(f,0); putU16(f,0); putU16(f,0); putU16(f,0); /* reserved2 */
   }

   /* --- COLLATE --- */
   for (i = 0; i < g_numCollateTables; i++) {
      putU8(f, 0xFF);
      putBytes(f, (const unsigned char *)"COLLATE", 7);
      putU8(f, g_collateTables[i].flag1);
      putU8(f, g_collateTables[i].flag2);
      putBytes(f, g_collateTables[i].table, 256);
   }

   /* --- UCASE / FUCASE --- */
   for (i = 0; i < g_numUcaseTables; i++) {
      putU8(f, 0xFF);
      putBytes(f, (const unsigned char *)g_ucaseTables[i].tag, 7);
      putU8(f, g_ucaseTables[i].length);
      putU8(f, g_ucaseTables[i].flag2);
      putBytes(f, g_ucaseTables[i].table, 128);
   }

   /* --- FCHAR --- */
   for (i = 0; i < g_numFcharTables; i++) {
      putU8(f, 0xFF);
      putBytes(f, (const unsigned char *)"FCHAR  ", 7);
      putU16(f, g_fcharTables[i].reclen);
      putBytes(f, g_fcharTables[i].body, g_fcharTables[i].reclen);
   }

   /* --- DBCS --- */
   for (i = 0; i < g_numDbcsTables; i++) {
      putU8(f, 0xFF);
      putBytes(f, (const unsigned char *)"DBCS   ", 7);
      putU16(f, g_dbcsTables[i].reclen);
      putBytes(f, g_dbcsTables[i].body, g_dbcsTables[i].reclen);
   }

   fclose(f);
   printf("\nWrote %s\n", outname);
   free(g_dbcsOffsets);
   return 0;
}
