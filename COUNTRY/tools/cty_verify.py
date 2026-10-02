#!/usr/bin/env python3
"""
cty_verify.py - check a generated COUNTRY.SYS against the real one.

    python cty_verify.py GENERATED.SYS REAL.SYS

For every directory key of the REAL file, looks the key up in the generated
file (first match, like the OS does) and compares the full CTYINFO record and
the COLLATE / UCASE / UCASE-or-FUCASE / FCHAR / DBCS leaf records byte for
byte. Also checks header, count, entry/table2 framing and offset ranges.
Exit status 0 = equivalent. Real-file quirk: key 51/850's table2 is
mis-anchored in the real file, so its leaves are compared via the re-anchored
slots (same rule as cty_extract.py); orphan CTYINFOs the directory never
reaches are not compared.
"""
import sys, struct

gen = open(sys.argv[1], "rb").read()
real = open(sys.argv[2], "rb").read()
U16 = lambda d, o: struct.unpack_from("<H", d, o)[0]
errs = []
def err(m): errs.append(m)

def leaf_len(d, o, typ):
    tag = d[o+1:o+8]
    if typ == 1: return 48
    if typ == 6: return 266
    if typ in (2, 4): return 138
    return 10 + U16(d, o+8)           # FCHAR / DBCS: reclen at +8

def table(d, key_idx):
    _, c, cp, z1, z2, toff, z3 = struct.unpack_from("<7H", d, 25 + key_idx*14)
    n = U16(d, toff)
    ents = [struct.unpack_from("<4H", d, toff + 2 + k*8) for k in range(n)]
    if n != 6:   # re-anchor on the CTYINFO slot (see docstring)
        k0 = [k for k, e in enumerate(ents) if e[1] == 1 and d[e[2]] == 0xFF
              and d[e[2]+1:e[2]+8] == b"CTYINFO"][0]
        ents = [struct.unpack_from("<4H", d, toff + 2 + (k0+j)*8) for j in range(6)]
    return (c, cp, z1, z2, z3, n, ents)

if gen[:23] != real[:23]: err("header differs")
n_real, n_gen = U16(real, 23), U16(gen, 23)
if n_real != n_gen: err(f"directory count real={n_real} gen={n_gen}")
if len(gen) > 0xFFFF: err("generated file exceeds 16-bit offsets")

checked = 0
for i in range(min(n_real, n_gen)):
    rc, rcp, *_ , rents = table(real, i)
    gc, gcp, gz1, gz2, gz3, gn, gents = table(gen, i)
    if (rc, rcp) != (gc, gcp): err(f"key #{i}: real {rc}/{rcp} gen {gc}/{gcp}"); continue
    if (gz1, gz2, gz3, gn) != (0, 0, 0, 6): err(f"key #{i}: bad framing")
    if [e[1] for e in gents] != [1, 6, 2, 4, 5, 7] or any(e[0] != 8 or e[3] for e in gents):
        err(f"key #{i}: bad table2 slots"); continue
    for r, g in zip(rents, gents):
        t = r[1]
        if g[2] + leaf_len(gen, g[2], t) > len(gen): err(f"key #{i}: type {t} leaf out of range"); continue
        a = real[r[2]: r[2] + leaf_len(real, r[2], t)]
        b = gen[g[2]: g[2] + leaf_len(gen, g[2], t)]
        if a != b: err(f"key #{i} {rc}/{rcp}: type {t} leaf differs")
    checked += 1

print(f"{checked}/{n_real} directory keys compared; {len(errs)} problem(s)")
for e in errs[:20]: print("  ", e)
sys.exit(1 if errs else 0)
