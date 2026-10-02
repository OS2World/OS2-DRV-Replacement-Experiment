#!/usr/bin/env python3
"""
cty_extract.py - regenerate build/cty_data.c from a real COUNTRY.SYS.

    python cty_extract.py COUNTRY.SYS build/cty_data.c [REFERENCE.SYS]

Walks the real file's documented structure (see ../PLAN.md Sec 9):
header(23) | count | directory(14-byte entries) | table2s | leaf records.
Emits every distinct leaf table once plus the 304 directory keys, so
ctybuild.c can reassemble an equivalent file. Pure data extraction - nothing
is hand-typed. With a 3rd argument it also writes a reference COUNTRY.SYS using the
same layout algorithm as ctybuild.c, so the C build can be compared byte-for-byte.
CTYINFO records that no directory key reaches (2 in the ArcaOS 5.1 file) are dropped.
Self-checks every offset/signature and aborts on any surprise.
"""
import sys, struct

if len(sys.argv) not in (3, 4):
    sys.exit(__doc__)
data = open(sys.argv[1], "rb").read()
out_c = sys.argv[2]
U16 = lambda o: struct.unpack_from("<H", data, o)[0]

assert data[0] == 0xFF and data[1:8] == b"COUNTRY", "not a COUNTRY.SYS"
dir_off = data[0x13]
assert dir_off == 23
dcnt = U16(dir_off)

TYPES = [1, 6, 2, 4, 5, 7]   # CTYINFO COLLATE UCASE UCASE/FUCASE FCHAR DBCS

def leaf_ok(off, sigs):
    return data[off] == 0xFF and data[off+1:off+8].rstrip() in sigs

# ---- directory -> table2 -> leaf offsets ----
dirents = []     # (keyCountry, keyCodepage, ctyinfoOffset)
tables = {}      # ctyinfoOffset -> {type: leafOffset}
for i in range(dcnt):
    sz, dc, dcp, z1, z2, toff, z3 = struct.unpack_from("<7H", data, dir_off + 2 + i*14)
    assert (sz, z1, z2, z3) == (12, 0, 0, 0), i
    n = U16(toff)
    ents = [struct.unpack_from("<4H", data, toff + 2 + k*8) for k in range(n)]
    # Real-file quirk: directory entry 51/850 points 48 bytes before its real
    # table2 (count word reads 8, first 6 slots are garbage). Re-anchor on the
    # type-1 slot and take the 6 slots from there.
    starts = [k for k, e in enumerate(ents) if e[0] == 8 and e[1] == 1 and
              leaf_ok(e[2], (b"CTYINFO",))]
    assert len(starts) == 1, (i, dc, dcp)
    k0 = starts[0]
    if n != 6:
        print(f"note: dir #{i} key {dc}/{dcp}: table2 count={n}, re-anchored at slot {k0}")
        ents = [struct.unpack_from("<4H", data, toff + 2 + (k0+j)*8) for j in range(6)]
    assert [e[1] for e in ents] == TYPES and all(e[0] == 8 and e[3] == 0 for e in ents), i
    tab = {e[1]: e[2] for e in ents}
    cty = tab[1]
    # a CTYINFO shared by several keys must have identical sibling tables
    assert tables.setdefault(cty, tab) == tab, (i, dc, dcp)
    dirents.append((dc, dcp, cty))

# ---- every CTYINFO in the file (record order = ascending file offset) ----
cty_all = []
p = 0
while True:
    p = data.find(b"\xffCTYINFO", p)
    if p < 0: break
    cty_all.append(p); p += 1
unref = [hex(c) for c in cty_all if c not in tables]
print(f"{dcnt} directory keys, {len(cty_all)} CTYINFO records, "
      f"{len(tables)} referenced; unreferenced: {unref}")
records = [c for c in cty_all if c in tables]
rec_idx = {c: i for i, c in enumerate(records)}

# ---- leaf readers ----
def rd_collate(o):
    assert leaf_ok(o, (b"COLLATE",))
    return (data[o+8], data[o+9], data[o+10:o+266])
def rd_ucase(o):
    assert leaf_ok(o, (b"UCASE", b"FUCASE"))
    return (data[o+1:o+8].decode().rstrip(), data[o+8], data[o+9], data[o+10:o+138])
def rd_fchar(o):
    assert leaf_ok(o, (b"FCHAR",)); n = U16(o+8)
    return (n, data[o+10:o+10+n])
def rd_dbcs(o):
    assert leaf_ok(o, (b"DBCS",)); n = U16(o+8)
    assert n <= 8
    return (n, data[o+10:o+10+n])

stores = {k: ([], {}) for k in "cufd"}   # lists + offset->index
def idx(kind, off, rd):
    lst, cache = stores[kind]
    if off not in cache:
        cache[off] = len(lst); lst.append(rd(off))
    return cache[off]
fch = []
recs = []
for c in records:
    t = tables[c]
    recs.append((c, idx("c", t[6], rd_collate), idx("u", t[2], rd_ucase),
                 idx("u", t[4], rd_ucase), idx("f", t[5], rd_fchar), idx("d", t[7], rd_dbcs)))

B = lambda arr: ",".join(str(b) for b in arr)
L = []
L.append('/* cty_data.c - extracted from the real ArcaOS 5.1 COUNTRY.SYS, programmatically,')
L.append(' * via byte-level format analysis (see ../PLAN.md). Not hand-typed. */')
L.append('#include "ctybuild.h"'); L.append("")
L.append(f"const COLLATE_TABLE g_collateTables[{len(stores['c'][0])}] = {{")
for f1, f2, t in stores['c'][0]: L.append(f"  {{ {f1}, {f2}, {{ {B(t)} }} }},")
L += ["};", ""]
L.append(f"const UCASE_TABLE g_ucaseTables[{len(stores['u'][0])}] = {{")
for tag, ln, f2, t in stores['u'][0]:
    L.append(f'  {{ "{tag.ljust(7)}\0", {ln}, {f2}, {{ {B(t)} }} }},')
L += ["};", ""]
L.append(f"const FCHAR_TABLE g_fcharTables[{len(stores['f'][0])}] = {{")
for n, body in stores['f'][0]: L.append(f"  {{ {n}, {{ {B(body)} }} }},")
L += ["};", ""]
L.append(f"const DBCS_TABLE g_dbcsTables[{len(stores['d'][0])}] = {{")
for n, body in stores['d'][0]:
    L.append(f"  {{ {n}, {{ {B(list(body) + [0]*(8-len(body)))} }} }},")
L += ["};", ""]
L.append(f"const int g_numCollateTables = {len(stores['c'][0])};")
L.append(f"const int g_numUcaseTables = {len(stores['u'][0])};")
L.append(f"const int g_numFcharTables = {len(stores['f'][0])};")
L.append(f"const int g_numDbcsTables = {len(stores['d'][0])};")
L.append("")
L.append(f"const COUNTRY_RECORD g_countries[{len(recs)}] = {{")
for c, ci, ua, ub, fc, db in recs:
    L.append(f'  {{ {U16(c+10)}, {U16(c+12)}, {U16(c+14)}, '
             f'{{{B(data[c+16:c+21])}}}, {{{B(data[c+21:c+23])}}}, {{{B(data[c+23:c+25])}}}, '
             f'{{{B(data[c+25:c+27])}}}, {{{B(data[c+27:c+29])}}}, '
             f'{data[c+29]}, {data[c+30]}, {data[c+31]}, {{{B(data[c+36:c+38])}}}, '
             f'{ci}, {ua}, {ub}, {fc}, {db} }},')
L.append("};")
L.append(f"const int g_numCountries = {len(recs)};"); L.append("")
L.append(f"const DIR_ENTRY g_dirEntries[{len(dirents)}] = {{")
for dc, dcp, c in dirents: L.append(f"  {{ {dc}, {dcp}, {rec_idx[c]} }},")
L.append("};")
L.append(f"const int g_numDirEntries = {len(dirents)};")
open(out_c, "w", newline="\n").write("\n".join(L) + "\n")
print(f"wrote {out_c}: {len(recs)} records, {len(dirents)} directory keys, "
      f"{len(stores['c'][0])} COLLATE, {len(stores['u'][0])} UCASE/FUCASE, "
      f"{len(stores['f'][0])} FCHAR, {len(stores['d'][0])} DBCS")

# ---- optional: reference file, same layout algorithm as ctybuild.c ----
if len(sys.argv) == 4:
    N, ND = len(recs), len(dirents)
    cl, ul, fl, dl = (stores[k][0] for k in "cufd")
    t2 = 25 + ND*14; cty = t2 + N*50; col = cty + N*48
    uc = col + len(cl)*266; fc = uc + len(ul)*138; db = fc + len(fl)*32
    dboff, o = [], db
    for n, body in dl: dboff.append(o); o += 10 + n
    assert o <= 0xFFFF, "file would exceed 64 KB (16-bit offsets)"
    b = bytearray(data[0:23]); b += struct.pack("<H", ND)
    for dc, dcp, c in dirents:
        b += struct.pack("<7H", 12, dc, dcp, 0, 0, t2 + rec_idx[c]*50, 0)
    for i, (c, ci, ua, ub, fc_, db_) in enumerate(recs):
        b += struct.pack("<H", 6)
        for t, off in ((1, cty+i*48), (6, col+ci*266), (2, uc+ua*138),
                       (4, uc+ub*138), (5, fc+fc_*32), (7, dboff[db_])):
            b += struct.pack("<4H", 8, t, off, 0)
    for c, *_ in recs:
        b += data[c:c+48]
    for f1, f2, t in cl: b += b"\xffCOLLATE" + bytes([f1, f2]) + t
    for tag, ln, f2, t in ul: b += b"\xff" + tag.ljust(7).encode() + bytes([ln, f2]) + t
    for n, body in fl: b += b"\xffFCHAR  " + struct.pack("<H", n) + body
    for n, body in dl: b += b"\xffDBCS   " + struct.pack("<H", n) + body
    assert len(b) == o
    open(sys.argv[3], "wb").write(b)
    print(f"wrote reference {sys.argv[3]}: {len(b)} bytes")
