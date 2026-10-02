# COUNTRY.SYS — format documentation and generator

`COUNTRY.SYS` is the OS/2 / ArcaOS country-information file (loaded by
`COUNTRY=nnn,path\COUNTRY.SYS` in CONFIG.SYS). It supplies the data behind
`DosQueryCtryInfo`, `DosQueryCollate`, `DosMapCase` and `DosQueryDBCSEnv`: currency,
date/time/number formats, sort weights, upper-casing tables and DBCS lead bytes for
every country/code page. Its byte layout was never publicly documented.

This project documents that layout and provides a **C generator
(`ctybuild`)** that builds a working `COUNTRY.SYS` containing the same data set.

`COUNTRY.SYS` is passive data, not a driver. A structurally invalid file halts boot with
`SYS02069` ("country information file ... not valid"), nothing worse. No IBM source was
used or exists for it.

## Status

Tested on an **ArcaOS 5.1.2** VirtualBox VM. **OS/2 Warp 4.52 has not been tested.**

- `build/COUNTRY.SYS` (built by `ctybuild.exe`: 289 records, 304 directory keys,
  50,485 bytes) boots with no `SYS02069`.
- `DosQueryCtryInfo` matches the real file for 1/850, 81/932, 49/850 and 90/857.
- `DosQueryCollate`, `DosMapCase` and `DosQueryDBCSEnv` work; 81/932 returns the Shift-JIS
  lead bytes `81-9F`, `E0-FC`; `DosMapCase` on 90/857 (Turkish) gives output identical to
  the real file.
- The C output is byte-identical to the independent reference writer in
  `tools/cty_extract.py`, and `tools/cty_verify.py` finds 0 differences against the real
  file across all 304 directory keys.

See `PLAN.md` (Sec 9-10) for the full test log.

## File format

Little-endian. All offsets are absolute file offsets in 16-bit fields, so the file must
stay below 64 KB (the real file is 50,947 bytes; the generator refuses to write more).

```
header      23 bytes   FF "COUNTRY" 01 00 00 00 00 00 00 00 | 01 00 01 | 17 00 00 00
                       (byte 0x13 = 23 = directory offset)
count       WORD       number of directory entries (304 in the ArcaOS 5.1 file)
directory   14 bytes each:  { 12, keyCountry, keyCodepage, 0, 0, table2Offset, 0 }
table2      50 bytes each:  { 6 } then 6 x { 8, type, leafOffset, 0 }
                       type order: 1 CTYINFO, 6 COLLATE, 2 UCASE, 4 UCASE/FUCASE,
                                   5 FCHAR, 7 DBCS
leaf records  all start FF + 7-char tag:
  CTYINFO  48 B  "CTYINFO", reclen=38, country, codepage, fsDateFmt, szCurrency[5],
                 4 x 2-byte separators (thousands, decimal, date, time),
                 fsCurrencyFmt, cDecimalPlace, fsTimeFmt, 4 reserved,
                 szDataSeparator[2], 10 reserved
  COLLATE  266 B "COLLATE", 2 flag bytes, 256-byte weight table
  UCASE    138 B "UCASE  " / "FUCASE ", 2 flag bytes, 128-byte case map for 0x80-0xFF
  FCHAR    32 B  "FCHAR  ", reclen=22, forbidden filename characters
  DBCS     10+n  "DBCS   ", reclen=n (2/4/6), lead-byte range pairs, 00 00 terminator
```

Notes:
- The directory key (what `COUNTRY=nnn,cp` asks for) is **not always the record's own
  country/codepage**: 22 of the 304 keys are aliases (e.g. `001/932` resolves to the Japan
  81/932 record). Many tables are shared between records.
- Type 4 is a normal UCASE table except for countries 65/81/82/86/88/90, where it is a
  `FUCASE` table (DBCS filename folding / Turkish i-rules).
- Real-file quirks not reproduced: key 51/850's table2 pointer is 48 bytes early (the count
  reads 8 and the first 6 slots are garbage), so we emit a normal table2 for it; two
  CTYINFO records are orphans that no directory key reaches, so they are dropped.
- OS/2 validates the structure at boot. The count word must equal the number of
  directory entries (an early attempt kept 304 while writing 291 entries and failed with
  `SYS02069`). Trailing padding after the last record is accepted.

## Layout of this folder

| Path | What |
|---|---|
| `build/ctybuild.c`, `ctybuild.h` | The generator (ordinary OS/2 console program, OpenWatcom `wcl386`) |
| `build/cty_data.c` | Generated data: every table, record and directory key, extracted from the real file |
| `build/COUNTRY.SYS` | The generated file (booted and tested on ArcaOS 5.1.2). Golden reference: a fresh `ctybuild` run must reproduce it byte for byte |
| `compile-build.cmd` | Builds `ctybuild.exe` on the ArcaOS VM (self-logging) |
| `tools/cty_extract.py` | Original `COUNTRY.SYS` -> `cty_data.c` (+ optional independent reference writer) |
| `tools/cty_verify.py` | Compares a generated file to the original one, key by key and byte by byte |
| `test/ctrytest.c`, `test/ctrytest2.c`, `compile-test.cmd` | VM-side API cross-checks |
| `experiments/` | Historical test files (patched, padded, broken first builds); see its `README.txt` |
| `PLAN.md` | Full research log, every experiment, dead ends |

The original IBM/Arca Noae `COUNTRY.SYS` is not distributed; it is only needed to re-run
the extractor or the verifier (see below).

## Build and use (on the OS/2 VM, OpenWatcom installed)

```
compile-build.cmd
ctybuild COUNTRY.NEW
```

(`ctybuild` writes `COUNTRY.NEW` by default.) You can use `build\COUNTRY.SYS` directly,
or the freshly built `COUNTRY.NEW`, which should be identical to it. Then set `COUNTRY=001,D:\path\COUNTRY.SYS` in CONFIG.SYS (use 8.3 names), reboot, and
cross-check:

```
compile-test.cmd
ctrytest 1 850          (also: 81 932, 49 850, 90 857)
ctrytest2 1 850         (also: 81 932, 90 857)
```

`ctrytest` prints the `COUNTRYINFO` fields; `ctrytest2` prints the collate table start,
`DosMapCase` results (including high-half bytes) and the DBCS lead-byte ranges. Keep your
original `COUNTRY.SYS` around to restore. A bad file will stop the boot, so test on a
disposable VM or keep boot media handy.

## Verify

On the VM, the build must reproduce the shipped file (host side, after copying it back):

```
cmp build/COUNTRY.NEW build/COUNTRY.SYS         # must be identical (comp on OS/2; no fc)
```

To check against your own copy of the original file (host; needs Python 3):

```
python tools/cty_verify.py build/COUNTRY.SYS <your-original>/COUNTRY.SYS
python tools/cty_extract.py <your-original>/COUNTRY.SYS build/cty_data.c build/COUNTRY.REF
cmp build/COUNTRY.REF build/COUNTRY.SYS         # reference writer vs shipped file
```

`cty_verify.py` finds 0 differences across all 304 directory keys for the shipped file.

## Caveats and licensing

- Code here: BSD-3-Clause (see `LICENSE`). Prior art: Wim Brul's `cty593ec.inc`
  (BSD-3-Clause).
- The original `COUNTRY.SYS` belongs to IBM / Arca Noae and is **not** distributed here;
  supply your own copy to run the extractor or verifier. `cty_data.c` and the generated
  `build/COUNTRY.SYS` contain locale facts (separators, currency, case and collation
  tables) derived from it, regenerated by this project's own code.
- The generator reproduces the ArcaOS 5.1 data set; it does not invent locales. To add or
  change a country, edit the data in `cty_data.c` (and rebuild).
