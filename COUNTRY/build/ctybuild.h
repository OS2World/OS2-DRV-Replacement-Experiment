/**
 * ctybuild.h - shared structures for the COUNTRY.SYS generator.
 *
 * Field layouts here come from two sources only, both legitimate:
 *  1. This project's own byte-level reverse-engineering of a real ArcaOS 5.1
 *     COUNTRY.SYS (see ../PLAN.md Sec 5) - cross-checked against live
 *     DosQueryCtryInfo output and empirically proven by a successful
 *     patch-and-boot test (PLAN.md Sec 6).
 *  2. Wim Brul's own experimental country-info file (BSD-3-Clause licensed,
 *     OS2World/SYSTEM-DRV-Wims_Experimental_Files on GitHub - his own work,
 *     not IBM material) - used to confirm/extend the leaf record formats and
 *     the type-dispatch table's type-code enumeration (PLAN.md Sec 7).
 *
 * No IBM source was read for this project - COUNTRY.SYS is a passive data
 * file with no IBM-released source anywhere (confirmed early in this
 * project), so unlike the sibling KBDBASE.SYS project there was never a
 * confidential-material concern here to begin with.
 */
#ifndef CTYBUILD_H
#define CTYBUILD_H

#ifdef __OS2__
#include <os2.h>
typedef UCHAR BYTE8;
#else
typedef unsigned char BYTE8;
typedef unsigned short USHORT;
typedef unsigned long ULONG;
#endif

#pragma pack(1)

/* COLLATE leaf record body (after the FF+'COLLATE'+len-placeholder header -
 * see ctybuild.c's writer, which emits the real FF/'COLLATE' signature
 * itself; this struct is just the 2 flag bytes + 256-byte table that follow
 * it in the file, matching PLAN.md Sec 7c exactly). */
typedef struct {
   BYTE8 flag1;      /* observed 0x00 in every real sample */
   BYTE8 flag2;      /* observed 0x01 in every real sample */
   BYTE8 table[256];
} COLLATE_TABLE;

/* UCASE leaf record body. `length` is the table size (observed always 0x80
 * = 128 in every real sample - the extended/codepage-specific byte range
 * 128-255; values 0-127 don't need per-codepage mapping).
 *
 * `tag` is the actual 7-character signature to write ("UCASE  " or
 * "FUCASE "/"FUCASE") - the real file's type=4 ("UCASE_B") slot sometimes
 * points to a FUCASE-tagged table instead of a second UCASE one, observed
 * for Singapore/Japan/South Korea/China/Taiwan/Turkey (DBCS filename
 * case-folding, or Turkish's dotted/dotless-I rules - see PLAN.md Sec 8).
 * Same 128-byte structure either way, just a different tag string. */
typedef struct {
   char  tag[8];     /* "UCASE  \0" or "FUCASE \0" - 7 chars + null */
   BYTE8 length;     /* observed 0x80 in every real sample */
   BYTE8 flag2;      /* observed 0x00 in every real sample */
   BYTE8 table[128];
} UCASE_TABLE;

/* FCHAR leaf record body (22 bytes total per PLAN.md Sec 7c, all real
 * samples share reclen=0x16=22 and identical content - genuinely a single,
 * system-wide table, confirmed via signature search: only 1 FCHAR record in
 * the whole real file). */
typedef struct {
   USHORT reclen;    /* observed 22 (0x16) in the one real sample */
   BYTE8  body[22];
} FCHAR_TABLE;

/* DBCS leaf record body - VARIABLE length in the real file (2, 4, or 6
 * bytes observed - see PLAN.md Sec 7c). Stored here in a fixed 8-byte
 * buffer (max observed length 6), with `reclen` giving the real length to
 * write; the generator only emits `reclen` bytes of `body`, not the full
 * buffer. */
typedef struct {
   USHORT reclen;
   BYTE8  body[8];
} DBCS_TABLE;

/* One (country, codepage) pair's full CTYINFO content plus indices into the
 * deduplicated shared-table arrays (g_collateTables etc., in cty_data.c) -
 * exactly mirroring how the real file shares one COLLATE/UCASE/FCHAR/DBCS
 * table across every country using the same codepage. */
typedef struct {
   USHORT country;
   USHORT codepage;
   USHORT fsDateFmt;
   BYTE8  szCurrency[5];
   BYTE8  szThousandsSeparator[2];
   BYTE8  szDecimal[2];
   BYTE8  szDateSeparator[2];
   BYTE8  szTimeSeparator[2];
   BYTE8  fsCurrencyFmt;
   BYTE8  cDecimalPlace;
   BYTE8  fsTimeFmt;
   BYTE8  szDataSeparator[2];
   int    collateIdx;   /* index into g_collateTables */
   int    ucaseAIdx;    /* index into g_ucaseTables - "type 2" slot */
   int    ucaseBIdx;    /* index into g_ucaseTables - "type 4" slot */
   int    fcharIdx;     /* index into g_fcharTables */
   int    dbcsIdx;      /* index into g_dbcsTables */
} COUNTRY_RECORD;

#pragma pack()

/* Defined in cty_data.c (generated, not hand-written - extracted
 * programmatically from the real file, see PLAN.md Sec 8). */
extern const COLLATE_TABLE g_collateTables[];
extern const UCASE_TABLE   g_ucaseTables[];
extern const FCHAR_TABLE   g_fcharTables[];
extern const DBCS_TABLE    g_dbcsTables[];
/* Directory lookup key -> record. The key (what COUNTRY=nnn,cp asks for) is not
 * always the record's own country/codepage (e.g. 001/932 -> Japan record). */
typedef struct {
   unsigned short country;
   unsigned short codepage;
   unsigned short recordIdx;
} DIR_ENTRY;

extern const COUNTRY_RECORD g_countries[];
extern const DIR_ENTRY g_dirEntries[];
extern const int g_numDirEntries;
extern const int g_numCollateTables;
extern const int g_numUcaseTables;
extern const int g_numFcharTables;
extern const int g_numDbcsTables;
extern const int g_numCountries;

#endif /* CTYBUILD_H */
