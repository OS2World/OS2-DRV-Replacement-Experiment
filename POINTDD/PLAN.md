# POINTDD.SYS open-source clone — plan

**Goal:** an open-source replacement for POINTDD.SYS (the OS/2 Pointer Device Driver),
compatible with OS/2 Warp 4.52 and ArcaOS 5. **Motivation (user, 2026-09-30):** IBM no
longer supports OS/2 and will not release POINTDD.SYS source — the community needs its
own implementation to keep maintaining the platform.

**First target (user, 2026-09-30): the VirtualBox ArcaOS dev VM only** (the same VM used
for the RTL8188EU WiFi driver project) — i.e. whatever video driver that VM actually runs
(VBoxVideo/SVGA), not a universal clone. Broader compatibility is a later phase, once a
working driver exists to generalize from.

---

## 0. What POINTDD.SYS is

The Pointer Device Driver. It draws the mouse pointer image on screen and owns the
pointer's position/shape state. It must load **before** any `MOUSEXX.SYS` driver (EDM2);
MOUSE.SYS is the device-*independent* hardware input driver (reads PS/2/serial mouse
events) and depends on POINTDD.SYS being present. Loaded via:

```
DEVICE=\OS2\BOOT\POINTDD.SYS
```

It natively draws the pointer in VIO text modes 0,1,2,3,7 and graphics modes D,E,F,10
(classic VGA). For any other graphics mode (i.e. any real SVGA/GRADD-era driver — which
is our actual target), **it delegates pointer drawing to the display driver** via a
documented callback protocol (see §2). This delegation path is the one we need to
implement correctly for the VBoxVideo/SVGA case.

---

## 1. Source-availability recon (2026-09-30) — no POINTDD.SYS source exists anywhere found

Checked three independent, differently-vintaged IBM DDK archives:

1. `C:\Temporal\1.- OS2\Projects\References\os2ddk2004\` — a 2004-era compilation merging
   many historical DDK CDs (VIDEO, INOUT, MOUSE, COMPRINT, DBCS, MASM60, COMVIDEO...).
2. `C:\Temporal\1.- OS2\Projects\References\OLDER\IBMDDK_OS2_1-0\` — the genuine original
   **1993 "IBM Device Driver Source Kit for OS/2 Version 1.0"** (same 3,493-file ISO also
   archived at `archive.org/details/os-2-cd-rom_202401`; user supplied the local extract).
3. osFree (the OS/2 open-source reimplementation project), checked directly on GitHub
   (`github.com/orgs/osfree-project/repositories`, 44 repos) — zero matches for
   "point"/mouse/pointer work of any kind.

**Result, all three: only the compiled `pointdd.sys` binary exists** (in os2ddk2004 at
`ZIP\COMVIDEO\DDK\video\rel\os2c\retail\base\bbsdd\os2\boot\pointdd.sys`), **never
source**, in any version, anywhere. Same for `VDEV\VMOUSE` (the related DOS-VDM virtual
mouse driver) — both DDKs have only its `.doc`/history files, never its `.asm`/`.c`.
This is a consistent, settled finding, not a hasty one — IBM evidently kept POINTDD.SYS
(and VMOUSE) source internal across OS/2's entire life, unlike MOUSE.SYS and the video
drivers, whose source **is** present in both DDKs.

A Google AI Overview claimed osFree "has" POINTDD work and that the 1993 ISO "includes
POINTDD source" — **both claims were checked directly and are false.** Don't trust an AI
Overview's specifics; verify.

**Conclusion:** this is a spec-driven, from-scratch reimplementation, not a port of
existing source (unlike the sibling WiFi RTL8188EU project, which ported real Linux
driver code). The good news is in §2 — the *protocol* POINTDD must speak is unusually
well documented, even without its own source.

---

## 2. The protocol — what IS documented (all citations verified 2026-09-30)

### 2a. Device names, load order, CONFIG.SYS (EDM2 — public docs, not RE)
- `POINTDD.SYS` before `MOUSE.SYS` before `COM.SYS`.
- Native VIO text modes 0,1,2,3,7; native graphics modes D,E,F,10; else delegates to the
  display driver.
- https://www.edm2.com/index.php/POINTDD.SYS , https://www.edm2.com/index.php/MOUSE.SYS

### 2b. Ring-3 IOCtl contract (`bsedev.h` — official public header, not reverse engineering)
Local copy: `C:\Temporal\1.- OS2\Projects\References\os2tk45\h\bsedev.h`
- `IOCTL_POINTINGDEVICE` = `0x07` — the `MOU_*` function codes (`MOU_SETPTRSHAPE`=0x56,
  `MOU_DRAWPTR`=0x57, `MOU_REMOVEPTR`=0x58, `MOU_SETPTRPOS`=0x59, `MOU_GETPTRPOS`=0x67,
  `MOU_GETPTRSHAPE`=0x68, `MOU_QUERYPOINTERID`=0x6B, etc. — full table at lines 140-171).
  This is the interface apps/PM use against MOUSE.SYS; POINTDD.SYS is the thing actually
  implementing pointer drawing/shape/position underneath it.
- `IOCTL_SCR_AND_PTRDRAW` = `0x03` — `PTR_GETPTRDRAWADDRESS`=0x72, `VID_INITCALLVECTOR`=0x73
  (lines 79-89). This is the **display-driver handoff**: how the physical video driver's
  pointer-draw callback gets registered/discovered.
- Structs: `PTRDRAWFUNCTION`, `PTRDRAWADDRESS`, `PTRDRAWDATA` (lines 612-719).

### 2c. `DISPLAY.pdf` — the real find: a dedicated official chapter on this exact contract
Local copy: `C:\Temporal\1.- OS2\Projects\References\os2ddk2004\Docs-PDF\DISPLAY.pdf`
(~6MB IBM DDK reference manual). Extracted via `pdftotext -layout` (plain `pdftotext` is
on PATH in this environment, in `/mingw64/bin`).

- **"Physical Device Driver Initialization" (around line 4791 of the extracted text)**:
  Category 3, **Function 73h**. Data packet = `DWORD Subfunction; DWORD Parameter1;
  DWORD Parameter2; WORD ReturnCode`. Subfunction 1 = "PM Fill Logical Device Block":
  the video driver hands back a **Call Vector Table** — a dispatch table of **far
  addresses, each callable from Ring 3** — via an LDT-alias mechanism (`SCR_ALLOCLDT`
  family, bsedev.h lines 81-87). This confirms the classic 16:16-segmented,
  dual-ring-0/ring-3-callable OS/2 driver calling convention, not a flat 32-bit one.
- Same section explicitly names **"the Mouse Pointer Draw device driver"** (= POINTDD)
  as responsible for saving/restoring EGA registers across DOS-session screen switches
  for advanced graphics modes D/E/F/10 — direct evidence of its VDM-session
  responsibilities too.
- **Video-device-handler declaration (around line 2190-2260)**: the `VIDEO_DEVICES`
  CONFIG.SYS/OS2.INI environment-variable mechanism. Confirms the **default pointer
  device driver name is literally `POINTER$`**, via the `PTRDEVP()`/`PTRDEVR()` keywords
  (protect-mode / real-mode name respectively) — a video handler can even declare its
  *own* pointer driver name instead of the default (e.g. `PtrDevP=XGAPTR$`). Default
  init IOCtl for a physical device driver = Function 0x73 (cross-confirms §2b's
  `VID_INITCALLVECTOR`=0x73).
- A large "Mouse-Independent Pointer Drawing Services" section (~line 6600-6760) turned
  out to document the **DOS-VDM virtual-mouse-driver** side (Show/Hide Pointer, Define
  Text/Graphics Pointer, Set Video Page — entry points the virtual video driver hands to
  the virtual mouse driver on DOS-session open), a parallel but distinct piece from
  native-session POINTDD. Relevant for VDM support later, not for the first milestone.

### 2d. `pointer.asm` — real IBM source for the *display driver's* half of the contract
e.g. `C:\Temporal\1.- OS2\Projects\References\os2ddk2004\ZIP\VIDEO\DDK\video\src\video\vga32\svga256\pointer.asm`
(same file also in the 1993 DDK at `...\SRC\VGA32\SVGA256\POINTER.ASM`, and an EGA/VGA
16-bit variant at `...\SRC\PMDISP\EGAFAM\EGAVGA\POINTER.ASM` in the 1993 kit, and an
8514 variant at `...\SRC\PMDISP\PPXY\8514\POINTER.ASM`).

This is real, legitimate reference — it's the *other side* of the contract POINTDD talks
to, analogous to how the WiFi project used the Linux driver as ground truth. Documents,
in working code + extensive header comments:
- AND/XOR pointer-mask model (4-state: 0/display, 1/inverse-display per the truth table
  at lines 33-41).
- 32×32 fixed pointer/icon size, hot-spot semantics (lines 43-56).
- The display driver owns pointer exclusion: it must hit-test its own drawing ops
  against the pointer's screen rectangle and suppress the pointer there (lines 58-71).
- `pointer_off(ptlX, ptlY)` / `draw_pointer(ptlX, ptlY)` — `void draw_pointer(LONG ptlX,
  LONG ptlY)`, hot-spot coordinates (lines 249-300).
- **Callable from both ring 0 and ring 3** — the code explicitly branches on `CS` RPL
  (lines 324-345) and maintains separate ring-0/ring-3 VRAM pointers
  (`pVRAMRing0`/`pVRAMInstance`), because it's reached both via the Call Vector Table
  (ring 3, direct from PM) and from kernel context.

### 2e. What's still unknown (no source, no manual coverage found yet)
- The exact internal state machine POINTDD itself runs (shape cache, current
  position/visibility, session-switch handling) — has to be designed from the external
  contract above, there's no spec for POINTDD's own internals.
- The precise mechanism by which MOUSE.SYS and POINTDD.SYS locate each other at driver
  level (no `AttachDD`/IDC strings found anywhere in the MOUSE.SYS source — `pdi.asm` in
  that source is a red herring, "PDI" there means the *hardware* PS/2 Pointing Device
  Interface, not a POINTDD link). EDM2 only documents the *load order* requirement, not
  the mechanism. Likely candidates to investigate in Phase 1/2: the ring-3 `MOU_*` IOCtl
  surface is what session-manager/PMGRE code actually calls against *both* drivers
  (i.e. they may not talk to each other directly at all — worth testing this hypothesis
  before assuming a missing IDC link).

---

## 3. Toolchain recommendation (flagged for user confirmation — the toolkit's own rule
is "lay out the choice, don't pick for them")

**Recommendation: the 16-bit DDK toolchain (MS C 6.0 + MASM + LINK)** — the exact one
already proven working for the WiFi RTL8188EU driver (`WiFi-RTL8188EU-ArcaOS\src\`,
`build.cmd`/`makefile`/`rtl_hdr.asm` pattern). Reasoning:
- POINTDD's documented contract (§2c/2d) is pure classic 16:16-segmented OS/2 driver
  convention — far pointers, an LDT-alias Call Vector Table, ring-0/ring-3 dual calling.
  That's exactly the model MS C 6.0 + the 16-bit DDK targets; there is no indication
  anywhere in the docs of a flat-32-bit interface.
  Base OS/2 components historically stayed 16-bit segmented `.SYS` drivers throughout
  OS/2's life (this is *why* the WiFi project's 16-bit toolchain worked at all) — OS/2's
  32-bit flat driver model (`Drv32kit`/`DRV32KIT`) is a later, optional, add-on
  mechanism (used by e.g. MultiMac), not what base components use.
- The EDM2-listed size of POINTDD.SYS (3,846 bytes for the 2002 v10.70 build) is
  consistent with a small segmented driver, not a 32-bit LX module.
- **We already have a working, debugged build loop for this exact toolchain and driver
  class** (16-bit device header in MASM, `Init`/`Strategy` dispatch in C, DevHelp calls,
  COM1 serial trace for debugging with no console) — reusing it materially de-risks
  Phase 1.

If the user would rather target OpenWatcom/32-bit instead (e.g. for easier local
cross-compilation, per `claude-os2-toolkit/recipes/choosing-a-toolchain.md`), that's a
real option too — just flag it before Phase 1 starts, since it changes the skeleton.

**Confirmed by user (2026-09-30): going with the 16-bit toolchain.**

### 3a. Toolchain location on the VM (verified 2026-09-30 from a full `dir C:\ /s`)

This is the same VM used for the WiFi project (confirmed by the presence of
`C:\Drv16`, `C:\Drv32`, `C:\MiniDDK`, `C:\WATCOM` matching that project's memory). The
real, currently-loaded stock driver is at `C:\OS2\BOOT\POINTDD.SYS`, 3,846 bytes,
matching EDM2's documented v10.70 build exactly.

Each toolchain piece below was individually verified present *and* internally
consistent (matched dates/sizes) — **do not swap pieces from different directories
without re-checking; two lookalikes turned out to be wrong tools under the right name**:
- `C:\MiniDDK\base\{h,inc}` — current/maintained headers (2013-dated `devhdr.h` /
  `devcmd.h` / `reqpkt.h` / `dhcalls.h` / `devhdr.inc`), newer than the 2004 DDK
  archive's 2003-dated copies of the same files.
- `C:\MiniDDK\base\lib` — `dhcalls.lib`/`rmcalls.lib`/`os2386.lib`.
- `C:\Desktop\DDK\IBM_OS2_DDK_2004\DDK\ZIP\MASM60\binb` — `MASM.EXE` + `LINK.EXE` as a
  **matched pair** from the same 1991 MASM 6.0 SDK snapshot (both dated Feb/Mar 1991).
  **Not** `C:\usr\bin`'s `link.exe` — that one is only 12,813 bytes, dated 2019: some
  unrelated Unix-style tool squatting on the name, not the real MS segmented-executable
  linker (would have silently produced a cryptic failure). **Not** `MiniDDK\base\tools`
  either — it has `masm.exe` but no `link.exe` at all.
- `C:\usr\bin` — `cl.exe` (+ its `c1.exe`/`c2.exe` passes, confirmed together as a
  matched set) and `nmake.exe`. Put *last* on PATH in `compile.cmd` specifically so its
  bogus `link.exe` is never found before the real one.

This is now encoded in `compile.cmd` (project root, not `src\` — user preference,
2026-09-30), which writes `compile.log` (appended each run, matching the sibling
`WarpJoy` project's convention) **and** shows the same output live on the console via
`tee` (confirmed present at `C:\usr\bin\tee.exe`, already on PATH) — it re-invokes
itself once internally to pipe its own output, since a `.cmd` can't pipe itself in a
single pass. It sources the DDK from these **local VM
paths**, not from the `D:\PROJECTS\...` shared folder at all — sidestepping the earlier
open question of how much of the host tree that share actually covers. Only the
project's own source comes via that share, at the path the user gave directly:
`D:\PROJECTS\DRIVERS\POINTDD\src`.

---

## 4. Blast-radius / safety — read before the first CONFIG.SYS change

This is **categorically riskier to iterate on than the WiFi driver**. The WiFi driver was
additive — worst case, no WiFi. POINTDD.SYS loads **unconditionally, very early in
CONFIG.SYS, at every boot**, before the GUI or even the console pointer exists. A hang or
trap in `Init` there means **the VM does not boot at all**, not just "no mouse."

Before the very first skeleton driver is ever added to the VM's live CONFIG.SYS:
1. **Back up the working CONFIG.SYS** (copy to CONFIG.SYS.BAK or similar) so it can be
   restored from the ArcaOS boot/maintenance menu.
2. **Confirm the Alt-F1 / maintenance-partition recovery path works on this VM** *before*
   it's needed — the WiFi project used this same recovery method; verify it's still
   viable on the current VM snapshot before relying on it here.
3. Only then add `DEVICE=...\POINTDD.SYS` (or a differently-named test driver first, if
   the CONFIG.SYS line ordering/relative-position can be safely probed with a decoy name
   before using the real one — worth considering for the very first load test).

### 4a. Swap-in test procedure — claiming the real `POINTER$` name (2026-09-30)

Prepared after the decoy-name (`PTRSKEL$`) build proved the `PTR_GETPTRDRAWADDRESS`
handler correct and crash-safe under a real caller. The remaining open question — does
the *real* system call this naturally — can't be answered under a decoy name nothing
looks for, so this is a deliberate, temporary **swap-in/observe/swap-back** experiment,
not a permanent change. Built at `src-wat-realname\PDCLONE.c` (+ `makefile` +
`compile-realname.cmd` at the project root) — identical logic to `src-wat\PTRSKEL.c`,
the *only* difference is `cDevName = "POINTER$"`. Deliberately a different source file
(not a parameter/flag on the existing one) and a different output filename
(`PDCLONE.SYS`, never `POINTDD.SYS`) so the real driver file on disk is never touched,
confused with, or at risk of being overwritten — the swap is done entirely by changing
which file `CONFIG.SYS`'s `DEVICE=` line points at.

**Why this is a bigger step than the decoy test:** two drivers can't both claim
`POINTER$`, so this *replaces* the real `POINTDD.SYS`'s line, not adds alongside it. This
build still only implements `Init`/`InitComplete`/the one IOCtl — none of `POINTER$`'s
other normal responsibilities (native VIO-mode drawing, the `MOU_*` category 7 surface a
real `MOUSE.SYS` might expect during its own init). Realistic worst case: a broken/
invisible mouse pointer for the test session (the `Init`/`InitComplete` logic itself is
exactly what already boot-tested safely) — but this is genuinely less proven than the
decoy test, hence swap-in/observe/swap-back with a full backup, not a lasting change.

**Procedure (test VM only, never the Dev VM):**
1. Confirm the Alt-F1/maintenance-partition recovery path works on *this* VM, freshly,
   before starting.
2. Back up `CONFIG.SYS` under a distinct name (e.g. `CONFIG.SYS.PDTEST`) — this is the
   real safety net; nothing else needs to be backed up, since `C:\OS2\BOOT\POINTDD.SYS`
   itself is never touched.
3. In `CONFIG.SYS`, find the real driver's line (confirmed present on this VM:
   `C:\OS2\BOOT\POINTDD.SYS` — find the exact existing line text yourself rather than
   assume it verbatim, in case of extra parameters/formatting). Replace that one line,
   in the same position (order relative to `MOUSE.SYS` matters), with:
   `DEVICE=D:\PROJECTS\DRIVERS\POINTDD\src-wat-realname\PDCLONE.SYS`
   (Not using `REM` or any other comment syntax to disable the original line — couldn't
   confirm OS/2 `CONFIG.SYS` comment syntax in our available docs, so the line is
   replaced outright, with the backup from step 2 as the actual safety net.)
4. Reboot. Watch for: does it boot at all? Is there a console/boot message from our
   driver? Does a desktop appear? Is there a visible mouse pointer (even if frozen/
   broken is an acceptable, informative outcome — a hang or trap is not)?
5. Check the COM1 trace for a `G03:72R` sequence (or similar) — confirms whether the
   real system called `PTR_GETPTRDRAWADDRESS` on us.
6. **Swap back immediately after observing, regardless of outcome:** restore the backed-
   up `CONFIG.SYS`, reboot, confirm normal operation resumes with the stock driver.

**★★ RUN 1 RESULT (2026-09-30) — MAJOR FINDING.** Booted clean under the real `POINTER$`
name: `POINTDD clone (SWAP-IN TEST): POINTER$ loaded OK.` on the boot console, full
desktop reached, mouse pointer visually present and apparently functional. (Caution
flagged before checking COM1: VirtualBox's own mouse-integration overlay can make a
guest pointer look like it's working even if the guest's own pointer-draw subsystem does
nothing — `PtrDrawStub` is an empty no-op, so "looks fine" alone doesn't prove our
handler was exercised.) User confirmed via `CONFIG.SYS`: `REM` **does** work as a
comment prefix on this ArcaOS (empirically confirmed, we couldn't find this documented
locally) — used to disable the original line cleanly, `PDCLONE.SYS` copied to
`C:\OS2\BOOT\` (local disk, not referenced from the `D:\` share — an even safer variant
of the procedure than originally written here).

**COM1 trace (`com.log`) settled it: `G05:48x G03:72R G05:48x G03:72s G05:48x G03:72s
G05:48x G03:72s`.** Decoded (`G<cat>:<func><result>`):
- **`G03:72` = Category 3 (`IOCTL_SCR_AND_PTRDRAW`) / Function 0x72
  (`PTR_GETPTRDRAWADDRESS`) — the real system genuinely calls this on `POINTER$` during
  normal boot, four times.** This conclusively answers the open question from earlier in
  Phase 2: yes, something really does call this naturally, exactly as `PDDREF.pdf`
  documented — no longer just theory from documentation.
- **First call: `R` (success, our handler answered — matches what `testptr.exe` already
  proved worked). Every call after that: `s` (our own `RPERR_LENGTH` rejection — buffer
  too short, `usDataLen < sizeof(PTRDRAWFUNCTION)`=10).** Different callers are sending
  different buffer sizes for the same IOCtl. Worth understanding precisely (log the
  *exact* `usDataLen`, not just short/not) before Phase 3 — could mean multiple distinct
  callers, an older/smaller expected struct, or a caller that doesn't need all our
  fields.
- **`G05:48`** — a second, unexplained IOCtl also hits us repeatedly (category 0x05,
  function 0x48), correctly rejected (`x`). Category 5 is `IOCTL_PRINTER` per the known
  constant table, which doesn't obviously fit a pointer driver — not yet understood, not
  blocking anything, noted for later.

**This is the project's USBPcap-moment equivalent** (from the WiFi RTL8188EU project) —
the first real, empirical confirmation of the actual live protocol, not just documentation
inference.

**RUN 2/3 (2026-09-30):** Run 2's log turned out to be cumulative across two boots
(VirtualBox raw-file serial capture appends by default) — Run 1's old-format entries
plus 3 new ones, all `usDataLen=0x0000`. **Run 3 cleared `com.log` and rebooted fresh
for an unambiguous single-boot trace: `G05:48x G03:72L0000P0000s` × 3 — every call that
boot used `usDataLen=0` exactly, and none succeeded.** This is a more consistent signal
than Run 1's single success, which may have been boot-timing-dependent rather than
reliable. A `usDataLen` of exactly zero, repeated consistently, looks like a
**capability probe** (query before committing to a real buffer) rather than "undersized
buffer" — we currently reject it outright with `RPERR_LENGTH`.

**RUN 4 (2026-09-30) — CRASHED, and it's the most informative result yet.** Tried
answering `usDataLen==0` with fake success instead of rejecting. TRAP 000D, but **not in
our module** — `"Exception in module: VBOXMOUS"` (VirtualBox's own mouse-integration
driver). Strong resulting explanation: `VBOXMOUS` probes `POINTER$` with a zero-length
buffer to check whether real pointer-draw integration is available; when we reject that
probe (as Runs 1–3 did), `VBOXMOUS` takes the rejection as "not available" and falls
back to its own independent mouse-cursor mechanism — likely explaining why the pointer
looked fine in every run despite our `PtrDrawStub` doing nothing at all. When we
answered with fake success instead, `VBOXMOUS` proceeded as if it had received a real
draw-routine address, used whatever uninitialized data was in its own (zero-length)
buffer as if it were valid, and crashed.

**Conclusion: Runs 1–3's outright rejection of short/zero buffers is correct and
necessary, not a limitation to work around — reverted the Run 4 change.** `PDCLONE.c`
is back to Runs 1–3's behavior (reject anything `< sizeof(PTRDRAWFUNCTION)`), now
understood as the *right* answer rather than provisional. This also reframes Phase 3:
on this VirtualBox/VBoxVideo target specifically, the classic POINTDD pointer-draw
delegation may not be what's actually responsible for the visible cursor at all — worth
keeping in mind before investing further effort in `PtrDrawStub` for *this* target.

**Recovery:** the test VM halted completely (`"The system is stopped"`) — needed a hard
reset, then Alt-F1/maintenance-partition recovery to restore the backed-up `CONFIG.SYS`.

**RUN 5 (2026-09-30) — CONFIRMED FIXED.** Rebuilt with the reverted logic, swapped in
again: boots clean, no crash. `com.log`: `G05:48x G03:72L0000P0000s` × 3 — identical
pattern to Run 3, now proven stable and repeatable. **This closes out the real-name
swap-in investigation for now.** Findings locked in:
- The real system (`VBOXMOUS`, VirtualBox's own mouse-integration driver) genuinely
  calls `PTR_GETPTRDRAWADDRESS` on `POINTER$`, exactly as documented.
- It probes with a zero-length buffer, expecting — and correctly handling — outright
  rejection; our `RPERR_LENGTH` response is the *correct* answer, not a gap to fill.
- On rejection, `VBOXMOUS` falls back to its own cursor mechanism, which is almost
  certainly what's actually been drawing the visible pointer in every test so far.

**Strategic implication for Phase 3:** on the VirtualBox/VBoxVideo target this project
is currently scoped to, the system already works correctly with our current (mostly
rejecting) implementation — real pointer-draw delegation isn't exercised by `VBOXMOUS`
at all. Real drawing capability likely still matters for the project's actual goal
(community use beyond this one VM, including real hardware with no VBox fallback to
lean on), but it is not required to make *this specific* environment functional.

**User decision (2026-09-30): push toward real drawing capability.**

### 4b. Phase 3 first increment — proving PtrDrawStub actually works (2026-09-30)

Before writing any real video-memory-writing logic, needed to validate that
`PtrDrawStub` can genuinely be invoked and receives correct parameters. The natural
approach — extend `testptr.exe` to call the returned `pfnDraw` address directly — was
deliberately avoided: that would mean a 32-bit ring-3 app invoking a 16:16 far pointer
across a bitness boundary, an unverified ABI question (and not representative of real
usage anyway — the real caller, `MOUSE.SYS`, is itself 16-bit, so it never needs any
16/32 thunking to reach this at all).

Instead: a **private, driver-own test-trigger IOCtl**, `PTR_TEST_INVOKE_DRAW` (Category
3/Function 0xF0 — an unused range, not part of the documented protocol, never sent by
any real caller). When `testptr.exe` calls it, our own 16-bit driver code calls
`PtrDrawStub` **directly, from within itself** — an ordinary same-segment call, no
cross-bitness risk at all — then reports back a call counter and the last `(ptlX,
ptlY)` received, proving real invocation and correct parameter passing rather than just
"the IOCtl didn't crash". Implemented in `src-wat\PTRSKEL.c` (routine dev/test, safe
under the `PTRSKEL$` decoy — **no real-name swap-in needed for this round**, deliberately
not touching `src-wat-realname\PDCLONE.c`, which stays as the confirmed-safe Run 5
reference) and `test\testptr.c` (calls it twice, expects the call count to increase by 1
each time).

**BUILT + TESTED CLEAN (2026-09-30): `callCount` 1→2, `lastX=100 lastY=200` both times,
no regression on `PTR_GETPTRDRAWADDRESS`.** `PtrDrawStub` is proven to genuinely execute
with correct parameters and correctly-persisting resident state.

**Step 2 (2026-09-30): first real video-memory write.** `Dev16lib.h` provides
`MapPhysToVirt(ULONG Adr, ULONG Size)` — "creates a PERMANENT mapping", returns a 16:16
far pointer — exactly the documented mechanism needed to safely access physical memory
from a 16-bit driver (no guessed/hardcoded-segment DOS-style access). Called once in
`StrategyInit` for the VGA text-mode buffer (physical `0xB8000`, 4000 bytes = one full
80×25 screen); `PtrDrawStub` now writes a fixed test attribute byte (`0x4F`) to the
top-left character cell and reads it straight back. **Deliberately made this
self-verifying rather than relying on a visual check** — `testptr.exe` prints the
readback value, proving the write genuinely happened independent of whether the current
session is even a true full-screen VIO session (the only kind that maps to real
`0xB8000` the way this assumes — a PM-windowed command prompt doesn't, and we don't
know which one is in use).

**TESTED (2026-09-30), round 1 — from a PM-windowed command prompt: no crash, but
`vgaByteReadback=0xFF` both times** — not the `0x4F` we wrote, and `before=0xFF` too (so
the write never changed anything observable). Added more diagnostics before guessing
further: the response now also reports the exact segment:offset `MapPhysToVirt` returned
(via a union, not a pointer-cast-and-shift — far-pointer-to-integer conversion is
implementation-defined, so this uses the same "read raw bytes through a union" technique
already proven throughout this project) and the byte's value **before** our write (not
just after).

Rebuilt with the extra diagnostics: `vgaPtr=0d70:0000` — a plausible, non-null selector
close to the driver's own code selector (`pfnDraw`'s `0d68`), consistent with normal
sequential LDT/GDT allocation at init, not a mapping failure. `before=0xFF` and
`after=0xFF` both calls — consistent with physical `0xB8000` being outside the VGA
adapter's **currently active memory map window** (Graphics Controller Memory Map Select)
while a PM/graphics-mode desktop owns the display: an open/unmapped bus read floats to
`0xFF` and writes are silently dropped, with no fault (hence no crash). This also matches
POINTDD's actual documented role — `PTR_GETPTRDRAWADDRESS`/`pfnDraw` is specifically the
text-mode VIO cursor-draw mechanism; PM/graphics-mode cursor drawing goes through the
display driver (GRADD) instead, never through this path.

**CONFIRMED (2026-09-30), round 2 — from a genuine full-screen VIO/BIOS session
(ArcaOS 5.1.2 "BIOS" window in VirtualBox):**
```
DosDevIOCtl(cat=3,func=0xF0) call 0 OK: callCount=3 lastX=100 lastY=200
  vgaPtr=0d70:0000  before=0x07  after=0x4f
DosDevIOCtl(cat=3,func=0xF0) call 1 OK: callCount=4 lastX=100 lastY=200
  vgaPtr=0d70:0000  before=0x07  after=0x4f
```
`before=0x07` is the standard default VGA text attribute (light gray on black) — exactly
what real, live text-mode VRAM should already hold — and `after=0x4f` is exactly what
`PtrDrawStub` wrote. **Root cause confirmed, not a driver bug: the write/readback path is
correct; `0xB8000` is only live when a true full-screen text-mode session owns the
display.** First confirmed real write to physical video memory from the driver.

**Step 3 (2026-09-30): real cursor draw/erase with save-under — written, not yet
built/tested.** Replaced the fixed test-cell write with: `EraseCursor()` (restores
whatever was saved under the current cursor cell, no-ops if nothing's drawn) and
`DrawCursorAt(row, col)` (saves the target cell's current char+attribute, then swaps the
attribute's foreground/background nibbles so the cursor stays visible against any
underlying color — classic text-mode technique). `PtrDrawStub(ptlX, ptlY)` now
bounds-checks against the real 80×25 grid (treating `ptlX`/`ptlY` as column/row — an
explicit, unconfirmed assumption, since no real caller exists yet to observe), then calls
erase-old → draw-new, so moving the cursor never permanently clobbers screen content.
`PTR_TEST_INVOKE_DRAW` now alternates between two fixed test cells (same row, columns 10
and 20) so a single test run exercises draw → move → erase-and-restore, and reports
enough state (`curChar/curAttr`, `savedChar/savedAttr`, `oldCellChar/oldCellAttr`) for
`testptr.exe` to verify the restore is byte-exact — the second call's restored old cell
must match the first call's save-under values exactly. `testptr.exe` updated to do this
comparison itself and print `SAVE-UNDER OK` or `SAVE-UNDER MISMATCH`.

**★★★★★★★ CONFIRMED (2026-09-30), from a genuine full-screen VIO/BIOS session:**
```
DosDevIOCtl(cat=3,func=0xF0) call 0 OK: callCount=3 lastX=10 lastY=5
  drawn cell:  char=0x6c attr=0x70 (saved under: char=0x6c attr=0x07)
  old cell after restore: char=0xff attr=0xff
DosDevIOCtl(cat=3,func=0xF0) call 1 OK: callCount=4 lastX=20 lastY=5
  drawn cell:  char=0xff attr=0xff (saved under: char=0xff attr=0xff)
  old cell after restore: char=0x6c attr=0x07
  SAVE-UNDER OK: restored cell matches call 0's save-under exactly.
```
`testptr.exe`'s own embedded check passed — call 1's restored old cell (`char=0x6c
attr=0x07`) exactly matches call 0's save-under values, a byte-exact round trip, not just
"no crash". **Also visually confirmed**: a screenshot of the full-screen session shows the
actual drawn cell as a visible glitch in the on-screen text (a corrupted/highlighted
character around row 5, column 20 — where the second call drew and nothing erased it
afterward, since the test ends after call 1). **Draw/erase/save-under mechanism confirmed
working end to end, both at the byte level and visually on screen.**

Minor open curiosity, not blocking: `callCount` started at 3 for call 0 rather than 1,
suggesting `testptr.exe` may have already run once earlier in the same boot. Doesn't
affect the save-under verification, which only compares the two calls within a single run
to each other.

**Multi-row movement test (2026-09-30) — written, not yet built/tested.**
`PTR_TEST_INVOKE_DRAW` now cycles through three fixed test cells instead of two:
`(col=10,row=5)` → `(col=20,row=5)` → `(col=15,row=10)`. The first transition is
same-row (as before); the second changes row AND column — a genuine multi-row/diagonal
move, exercising `VGA_CELL_OFFSET`/`EraseCursor`/`DrawCursorAt` with a row change for the
first time (the offset math already generalized to this, just hadn't been tested).
`testptr.exe` now loops 3 times and checks the save-under round-trip for every
consecutive pair (`i>=1` against the previous call), not just once.

**★★★★★★★★ CONFIRMED (2026-09-30), from a genuine full-screen VIO/BIOS session:**
```
DosDevIOCtl(cat=3,func=0xF0) call 0 OK: callCount=1 lastX=10 lastY=5
  drawn cell:  char=0x6c attr=0x70 (saved under: char=0x6c attr=0x07)
  old cell after restore: char=0xff attr=0xff
DosDevIOCtl(cat=3,func=0xF0) call 1 OK: callCount=2 lastX=20 lastY=5
  drawn cell:  char=0x6e attr=0x70 (saved under: char=0x6e attr=0x07)
  old cell after restore: char=0x6c attr=0x07
  SAVE-UNDER OK: restored cell matches call 0's save-under exactly.
DosDevIOCtl(cat=3,func=0xF0) call 2 OK: callCount=3 lastX=15 lastY=10
  drawn cell:  char=0x63 attr=0x70 (saved under: char=0x63 attr=0x07)
  old cell after restore: char=0x6e attr=0x07
  SAVE-UNDER OK: restored cell matches call 1's save-under exactly.
```
Both save-under round-trips pass, including call 2's — the row-changing move from
`(20,5)` to `(15,10)` — confirming the mechanism works for a genuine multi-row/diagonal
move, not just horizontal sliding. `callCount` started at 1 this time (fresh boot, no
prior run) — confirms the earlier session's starting-count oddity (callCount=3 at call 0)
was just a leftover extra run before the screenshot was taken, not a real bug.
**Multi-row movement confirmed working.**

**Cleanup on unload (2026-09-30) — written, not empirically tested.** `STRATEGY_DEINSTALL`
split out from the `STRATEGY_OPEN`/`STRATEGY_CLOSE`/`STRATEGY_SAVERESTORE` no-op group and
now calls `EraseCursor()` before returning, so a dynamic unload restores the screen rather
than leaving a stray highlighted cell behind. Deliberately **not** done on
`STRATEGY_CLOSE` — that fires on every ordinary `DosClose`, not on the driver actually
going away, and a real pointer must stay visible regardless of which app opens/closes the
device; erasing there would be a correctness bug, not a safety improvement. **Caveat:**
no straightforward way exists to empirically trigger `STRATEGY_DEINSTALL` for a boot-time
`CONFIG.SYS DEVICE=` driver like this one — that strategy code exists mainly for
hot-pluggable/dynamically-removable drivers. Implemented per the documented DDK contract
for correctness, not yet observed live; flagged rather than silently assumed-correct.
**Rebuilt clean (2026-09-30), no compile errors/regressions.**

### 4c. Phase 4 — does the real system ever call our pfnDraw? (2026-09-30)

**Scope correction first:** the original Phase 4 roadmap wording (§5 below, kept for
history) said "implement the `MOU_*` ring-3 IOCtl surface ... against real MOUSE.SYS
input." That's wrong as stated — `MOU_*` (`IOCTL_POINTINGDEVICE`, category `0x07`) is
`MOUSE$`'s own *logical*-driver interface, what ring-3 apps/PM call to talk to
`MOUSE.SYS` — not something `POINTDD.SYS` implements or receives. `MOUSE.SYS` is the
*caller* of our `PTR_GETPTRDRAWADDRESS`-obtained `pfnDraw`, never the other way around.
Building the `MOU_*` surface would mean writing a `MOUSE.SYS` clone — a separate driver
project, out of POINTDD's actual scope. User chose instead (given three options) to reuse
the proven swap-in/observe/swap-back procedure (§4a) with the new draw code, to settle the
real open question directly: **does the real system ever actually call our `pfnDraw`
during live mouse movement, or does VBOXMOUS's own fallback (§4a Run 4 finding) mean it
never does?**

Ported PTRSKEL.c's proven Phase 3 draw/erase/save-under logic into
`src-wat-realname\PDCLONE.c` (bounds-checked to the real 80×25 text screen, same as
there) — this build and `PTRSKEL.c` are no longer byte-identical (a deliberate, documented
departure from §4a's original "only `cDevName` differs" framing). Since there's no ring-3
test app driving this build the way `testptr.exe` drives `PTRSKEL$`, added a COM1 trace at
the very top of `PtrDrawStub` itself — `'D'` + 8 hex digits of `ptlX` + `':'` + 8 hex
digits of `ptlY`, logged **before** the bounds-check, then `'Y'` (accepted, drawn) or
`'N'` (out of range, ignored). This is the only way to observe a real invocation, and
logging the raw value before bounds-checking also answers a second open question for
free: whether a real caller sends character-CELL coordinates (our assumption, 0-79/0-24)
or PIXEL coordinates (the documented unit for `pointer.asm`'s graphics-mode
`draw_pointer`, whose signature we copied without confirming its unit applies here too).
Also added `STRATEGY_DEINSTALL` → `EraseCursor()`, matching PTRSKEL.c's cleanup-on-unload.

**Written, not yet built/tested.** Next action: user runs `compile-realname.cmd`, then
follows the SAME swap-in procedure as Runs 1-5 (§4a steps 1-6: confirm recovery path,
back up `CONFIG.SYS`, replace the real driver's line with
`DEVICE=D:\PROJECTS\DRIVERS\POINTDD\src-wat-realname\PDCLONE.SYS` — or copy `PDCLONE.SYS`
to `C:\OS2\BOOT\` and `REM` the original, the safer variant Run 1 actually used — reboot).
After reboot, move the mouse around for a while — both in the PM desktop and in a
full-screen VIO/text session if possible, to maximize the chance of catching a real call
in whichever context (if any) actually triggers it — then check `com.log` for any `'D'`
entries. **Swap back immediately after observing, regardless of outcome**, per the
established discipline.

**First attempt (2026-09-30): inconclusive — `com.log` was stale/cumulative.** User
reported a clean boot, no visible problems. But the log checked contained `G03:F0T`
entries — the private `PTR_TEST_INVOKE_DRAW` function that exists ONLY in the decoy
build (`PTRSKEL.c`), never in `PDCLONE.c` — proving at least part of the log predates this
boot (same cumulative-log issue as §4a Run 2). No `'D'` marker anywhere in it either,
meaning no signal at all about this round, not even `PDCLONE.c`'s own expected boot-time
`PTR_GETPTRDRAWADDRESS` activity. **Needs Run 3's remedy: power off the VM, delete
`com.log` on the host, power on fresh, move the mouse around, then check again for an
unambiguous single-boot trace.**

**★★★★★★★★★ CLEAN RETEST (2026-09-30) — CONCLUSIVE. `com.log`:**
```
G05:48x G03:72 L0000 P0000 s   (×4)
```
All 4 `PTR_GETPTRDRAWADDRESS` calls this boot used `usDataLen=0` (the zero-length
capability-probe pattern) and **all 4 were rejected** — unlike Run 1, where the first
call happened to succeed. **Zero `'D'` entries anywhere, despite real mouse movement
during the session (both PM desktop and attempted full-screen context).** `PtrDrawStub`
was never invoked.

**This is self-explanatory and conclusive: if every probe this boot was rejected, nobody
ever received a valid `pfnDraw` address, so nothing could ever call it.** Confirms the
Run 4 hypothesis directly, not just as a plausible guess: **VBOXMOUS probes with a
zero-length buffer, treats rejection as the expected/normal outcome, and then draws the
cursor entirely through its own VirtualBox-integration mechanism. Real POINTDD
pointer-draw delegation is never exercised on this VirtualBox/VBoxVideo target while
Guest Additions mouse integration is active — this is not a bug in our driver, it's how
this specific environment actually behaves.**

**Implication for seeing real tracking work:** this environment can't validate live
`PtrDrawStub` invocation as long as VBOXMOUS's probe-and-fallback path is in play.

**Follow-up test (2026-09-30): disabled `VBoxMouse.sys`, tried VirtualBox Pointing
Device = USB Tablet (no movement — confirms USB Tablet needs VBoxMouse's absolute-
position translation, `USBMOUSE.SYS` alone can't interpret it), then PS/2 Mouse +
enabled `AMOUSE.SYS` (third-party alternative driver, `C:\OS2\BOOT\AMOUSE.SYS`, was
present but `REM`'d out — this `CONFIG.SYS` has no stock `MOUSE.SYS` at all). Mouse
moved correctly this time.**

**★★★★★★★★★★ SECOND INDEPENDENT CONFIRMATION — com.log:**
```
G05:48x G03:72 L0000 P0000 s   (×3)
```
Same exact pattern as the VBoxMouse test: every `PTR_GETPTRDRAWADDRESS` probe this boot
used `usDataLen=0` and was rejected. **Zero `'D'` entries, despite the mouse genuinely
moving.** `AMOUSE.SYS` — a completely different, third-party driver implementation —
exhibits the identical probe-and-fallback behavior as `VBoxMouse.sys`: treats rejection
as the expected/normal outcome and draws the cursor through its own internal mechanism,
never completing real delegation to `POINTER$`.

**Conclusion: this isn't a VirtualBox-specific quirk. Two unrelated "mouse subsystem"
implementations on this system both bypass real POINTDD pointer-draw delegation
entirely.** The classic `PTR_GETPTRDRAWADDRESS` handshake appears to function, in
practice on this system, as a capability probe that's expected to fail, not a live
rendering path — at least for every caller available to test here. Whether the genuine
*original* stock IBM `MOUSE.SYS` (if sourced/installed) would behave differently remains
untested and would be the only way to settle that further; not pursued yet, and may not
be worth the detour given two independent confirmations already.

**★ PHASE 4 CLOSED OUT (2026-09-30).** Given user's choice between pursuing a third
(stock `MOUSE.SYS`) test, leaving the AMOUSE/PS2 config in place, or treating the
question as settled — **chose to settle it and restore the normal configuration.**
Reverted: VirtualBox Pointing Device back to USB Tablet; `CONFIG.SYS` — `VBoxMouse.sys`
un-`REM`'d (restored), `AMOUSE.SYS` re-`REM`'d (disabled again), real `POINTDD.SYS`
un-`REM`'d (restored), `PDCLONE.sys` `REM`'d out (real-name swap-in test build disabled,
per standing discipline — never left active). `PTRSKEL.sys` (decoy) left loaded, harmless
and additive. **User confirmed: desktop comes up normally with working mouse
integration, same as before any of this started.**

**Phase 4 final answer:** the draw/erase/save-under mechanism built in Phase 3 is
correct and proven (byte-exact, visually confirmed, multi-row-capable) — but on this
specific test system, no available "mouse subsystem" caller (VirtualBox's own
`VBoxMouse.sys`, nor the third-party `AMOUSE.SYS`) ever completes the
`PTR_GETPTRDRAWADDRESS` handshake for real; both treat it purely as a capability probe
expected to fail, then draw the cursor through their own internal mechanism. Real,
observed invocation of `PtrDrawStub` by a live caller remains unproven in this
environment — not because the driver is wrong, but because nothing currently available
to test against actually exercises that path.

---

## 5. Proposed phases

- **Phase 0 (done, 2026-09-30):** this recon. Source-availability settled, protocol
  contract mapped from official docs, scope + toolchain recommendation set.
- **Phase 1 — skeleton that loads safely — COMPLETE (2026-09-30):** boot-tested on a
  separate ArcaOS 5 VM (deliberately not the Dev VM, to protect it from any boot-time
  risk — good practice, keep doing this for future boot tests). The OpenWatcom/Drv16Kit
  build's boot-console message appeared exactly as designed
  (`POINTDD clone (Phase 1, OpenWatcom): PTRSKEL$ loaded OK.`), and the system booted
  and ran normally afterward — confirms `Init`/`InitComplete` complete cleanly and the
  driver doesn't interfere with anything else. **Both toolchains proven: build *and*
  safe boot-load, not just compile/link.** `src\PTRSKEL.OS2` (551 bytes, MS C 6.0 +
  MASM + LINK, see §3a) and `src-wat\PTRSKEL.sys` (2,650 bytes, OpenWatcom + Drv16Kit,
  see §3b) both built clean. Two real `strat.c` bugs found+fixed on the MS-toolchain
  side along the way: missing `#include <strat2.h>` (needed by `reqpkt.h` for
  `P_DriverCaps`), and missing forward declarations for `ResidentEnd`/
  `g_ResidentDataEnd` (used in `Init()` before their definition later in the same file —
  unlike the WiFi driver, where these lived in a separate file with a shared-header
  `extern`).

### 3b. Parallel OpenWatcom track (user request, 2026-09-30: prefer open-source tools)

`References\Drv16\` (David Azarewicz's Drv16Kit — genuinely open, actively-used,
requires only an IBM DDK license per its own ReadMe, and the same kit family behind
several real `DRV-NET-*` reference drivers) turns out to be a **complete, proven, C-only
16-bit driver-building kit for OpenWatcom** — not just a bare compiler. It supplies the
whole device-header/Strategy-dispatch boilerplate as a linkable library module
(`Drv16.lib(header)`), so there's **no hand-written MASM device header at all** — a real
advantage over the `src\` MS-toolchain version's hand-built `ptr_hdr.asm`. It also
answers the open question from §3 about whether `wlink` can produce the correct 16-bit
segmented `.SYS` format: yes — `system os2 dll` / `format os2 dll` is exactly what the
kit's own proven `Required\makefile` template uses.

Ported at `src-wat\` (`driver.c`, `makefile`) + `compile-wat.cmd` at the project root —
adapted directly from the kit's own `Required\Driver.c`/`Required\makefile` samples
(the kit's "bare minimum driver" template already does almost exactly what Phase 1
needs), same `PTRSKEL$` decoy name as the `src\` version. Also uses the kit's own
`cprintf()` for the loaded-OK message — documented as using `DosPutMessage`, safe for
`DEVICE=` drivers at init time, prints straight to the boot console (no COM1 capture
setup needed, unlike the `src\` version's hand-rolled `tcom()`).

Environment: `WATCOM=C:\WATCOM`, `DDK=C:\MiniDDK` (the kit's ReadMe recommends MiniDDK
specifically), `DRV16KIT=C:\Drv16\Kit` — all confirmed present locally on this VM.

**BUILT CLEAN (2026-09-30): `src-wat\PTRSKEL.sys`, 2,650 bytes, no compile/link errors.**
One build-system bug found+fixed on the way: `wmake`'s `.c.obj`/`.obj.sys` inference
rules chain by matching *base filename*, not by following the `all:` target — the
source had to be named `PTRSKEL.c` (not `driver.c`) to match the `PTRSKEL.sys` target,
exactly as the kit's own sample names `Driver.c` to match `Driver.sys`.

**Both toolchains now produce a working Phase 1 skeleton** (`src\PTRSKEL.OS2`, 551
bytes vs. `src-wat\PTRSKEL.sys`, 2,650 bytes — size difference expected, the Drv16Kit
build pulls in more runtime). Given the user's stated preference for open-source tools,
and this build's real advantages (no hand-rolled MASM, kit-native `cprintf` console
message, a kit already proven via real MultiMac drivers), **the OpenWatcom/Drv16Kit
track (`src-wat\`) is now the primary active-development target; `src\` (MS toolchain)
stays frozen as the proven Phase 1 reference, not updated every phase going forward.**

- **Phase 2 — implement `PTR_GETPTRDRAWADDRESS` (IN PROGRESS, 2026-09-30):** corrected
  understanding from the original "Function 73h handshake" wording above (kept for
  history) — per `PDDREF.pdf` (Category `IOCTL_SCR_AND_PTRDRAW`/0x03, Function
  `PTR_GETPTRDRAWADDRESS`/0x72): *"This function is used by the mouse subsystem to
  obtain the entry point address of the pointer draw routine ... supported by the
  physical Pointer Draw device driver."* **We answer this call — we don't make it.**
  Searched for a reference implementation of whatever calls it (checked MOUSE.SYS's
  full source and the rest of the DDK for `GETPTRDRAWADDRESS`/`SETPROTDRAWADDRESS`/
  `SETREALDRAWADDRESS`) — nothing, only header constants. No released source exists for
  this orchestration layer, same pattern as POINTDD.SYS itself. Considered using `kdb`
  (the kernel debugger) to observe a real boot doing this handshake live — user approved
  the technique in principle, but it needs a debug `OS2KRNL` matching ArcaOS 5's actual
  kernel version, and only incompatible 1992-1994 ones exist locally (checked the VM
  thoroughly, and the real `C:\OS2\` system root). Not pursued further for now — user
  chose to implement our side and observe empirically instead.

  **BUILT CLEAN (2026-09-30): `src-wat\PTRSKEL.sys`, 3,170 bytes** (up from 2,650 for
  Phase 1, consistent with the added handler code), no compile/link errors.

  Since `PTRSKEL$` is a decoy name nothing in the real system queries for pointer-draw
  capability, simply booting this never exercises the new code at all — so a `test\`
  component was added: `testptr.c`, an ordinary 32-bit OS/2 console app (built via
  `compile-test.cmd`, `wcl386 -bt=os2 -l=os2v2` per the top-level `CLAUDE.md`'s console-
  app recipe — NOT a driver) that calls `DosOpen("PTRSKEL$")` then `DosDevIOCtl`
  directly with Category 3/Function 0x72, under our own control, and prints the raw
  response bytes. Same pattern as the Drv16Kit's own `Sample\test.c` (testing a sample
  driver's IOCtl interface).

  Implemented in `src-wat\PTRSKEL.c`: `STRATEGY_GENIOCTL` traces every GenIOCtl that
  arrives (category/function) and answers Category 3/Function 0x72 with a
  `PTRDRAWFUNCTION`-shaped response (struct cross-confirmed against
  `os2tk45\h\bsedev.h:612`, defined locally rather than `#include`d since `bsedev.h`
  assumes the ring-3 `os2.h` umbrella) pointing at `PtrDrawStub` — a deliberately safe
  no-op placeholder (no OS/DevHelp calls) with the same signature as `pointer.asm`'s
  `draw_pointer(LONG,LONG)`, since it plays the same conceptual role and that's our best
  evidence for the expected calling convention, not a proven fact. Must live in the
  driver's permanent code segment, not the discardable `_inittext` one `StrategyInit`
  uses — it can be called long after boot.

  **BUILT, then TRAPPED (2026-09-30): TRAP 000D (GPF) the first time `testptr.exe`
  actually triggered `STRATEGY_GENIOCTL` post-boot.** Root cause: the handler originally
  used `iprintf()`, on the assumption that its doc ("works for both `DEVICE=` and
  `BASEDEV=`") meant it was safe any time post-init. On closer reading, `iprintf` wraps
  `DevHelp_Save_Message`, whose own documentation (`PDDREF.pdf`) says *"the message is
  not displayed immediately, but is queued until **system initialization** retrieves it
  from the system message file"* — a boot-time-only mechanism too, just usable by both
  driver types during *their* init, not a general post-init logger. The trap screen's
  `DSLIM=0000010a` was the key diagnostic clue — it exactly matched `PTRSKEL.map`'s
  boundary between resident data and the discardable `_INITDATA` segment, consistent
  with touching something whose residency wasn't correctly accounted for once the
  kernel shrinks segments post-init. **Fixed** by replacing both `iprintf` calls with
  the proven-safe WiFi-project technique: raw COM1 UART tracing via
  `PortInByte`/`PortOutByte` (already provided by `Dev16lib.h` — no hand-rolled inline
  asm needed), which touches no DevHelp/kernel-message machinery at all. `cprintf`
  (used in `StrategyInit`, called only *during* init while segments are still
  full-size) is unaffected and stays as-is. **Not yet rebuilt/retested.**

  **RETESTED CLEAN (2026-09-30): no crash.** `testptr.exe` output:
  `DosOpen(PTRSKEL$) OK` → `DosDevIOCtl(cat=3,func=0x72) OK, dataLenOut=10` (exactly
  `sizeof(PTRDRAWFUNCTION)`) → `usReturnCode=0x0000`, `pfnDraw=0e50:0047` (selector
  `0e50` matches the driver's own code segment — the same selector that appeared as
  `CS` in the earlier trap screen — confirming this is a genuine, valid address inside
  `PTRSKEL`'s resident code, i.e. `PtrDrawStub`'s real location), `pchDataSeg=0000:0000`
  (matches the explicit placeholder). **The `PTR_GETPTRDRAWADDRESS` handler is proven
  correct and safe under real-world conditions — a real ring-3 caller, post-boot, no
  crash.** This closes out the "implement our side, test empirically" plan from earlier
  in Phase 2.

- **Phase 3 — actual pointer draw:** `PtrDrawStub` confirmed able to genuinely write and
  persist a byte in physical VGA text VRAM (§4b, round 2, 2026-09-30) when a real
  full-screen VIO/text session owns the display. Remaining work: replace the fixed
  test-cell/test-attribute write with a real cursor-cell draw/erase routine driven by
  `(ptlX, ptlY)`, decide the save-under/restore-previous-cell mechanism (needed so drawing
  the cursor doesn't permanently clobber screen content), and only then revisit whether
  VBoxVideo/SVGA's own Call Vector Table (Function 73h) matters for the graphics-mode case
  (deferred — PM/graphics-mode cursor drawing goes through the display driver, not this
  path, so may not be this driver's job at all; see §4b round 1 finding).
- **Phase 4 — does the real system ever call our `pfnDraw`? (CORRECTED SCOPE, see §4c):**
  the `MOU_*` wording above was wrong — that's `MOUSE$`'s own logical-driver interface,
  not something `POINTDD.SYS` implements. Actual Phase 4: swap-in test (§4a procedure)
  with the new draw/erase/save-under code ported over, COM1-tracing every real
  `PtrDrawStub` invocation, to find out whether `MOUSE.SYS` (or whatever obtained our
  address) ever actually calls it during live mouse movement — see §4c for the full
  writeup and current status.
- **Phase 5+ (later):** session-switch/VDM handling, broader video-driver compatibility
  beyond the dev VM, real ArcaOS hardware, Warp 4.52.

Same build/test discipline as the WiFi project throughout: edit on Windows, build on the
VM, reboot, read the COM1 serial trace — with the extra CONFIG.SYS-safety discipline from
§4 layered on top given the higher blast radius.
