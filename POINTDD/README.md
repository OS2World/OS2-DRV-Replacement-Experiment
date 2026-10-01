# POINTDD.SYS Clone

An open-source, from-scratch reimplementation of OS/2's **Pointer Device Driver**
(`POINTDD.SYS`) — the 16-bit physical device driver responsible for drawing the mouse
pointer on screen, targeting OS/2 Warp 4.52 and ArcaOS 5.

## Why this exists

IBM no longer supports OS/2, and the source code for `POINTDD.SYS` has never been
released. This project checked three independent archives looking for it — the original
1993 "IBM Device Driver Source Kit for OS/2 Version 1.0", the 2004 DDK compilation, and
the osFree project on GitHub — and found no trace of it in any of them. (Be wary of AI
search summaries claiming otherwise; one surfaced during this project's research
incorrectly claimed osFree has `POINTDD` source and that the 1993 ISO includes it — both
checked directly and are false.)

Since IBM will not release the source and will not support the driver going forward, this
project reimplements it from scratch against the **documented protocol** — not a
disassembly, not a port, no reverse engineering of the original binary. Every behavior
here is either sourced from public IBM documentation (`PDDREF.pdf`, `DISPLAY.pdf`,
`bsedev.h`) or empirically observed by building a test driver and watching what the real
system actually does, the same way you'd characterize any undocumented hardware
interface.

## Status

Developed and tested against an ArcaOS 5.1.2 VirtualBox VM. See
[`PLAN.md`](PLAN.md) for the full research/development log — protocol findings with
citations, every build/test iteration, and all empirical results in detail. Short
version:

| Phase | What | Status |
|---|---|---|
| 0 | Protocol research, source-availability check, scope decision | Done |
| 1 | Driver skeleton loads and boots cleanly (two toolchains) | Done |
| 2 | `PTR_GETPTRDRAWADDRESS` handler, proven correct under real callers | Done |
| 3 | Real cursor draw/erase/save-under in VGA text-mode VRAM | Done, verified byte-exact and visually, including multi-row moves |
| 4 | Does any real caller actually invoke the draw routine? | Answered — see "Key finding" below |
| 5+ | Session-switch/VDM handling, broader compatibility, real hardware, Warp 4.52 | Not started |

### Key finding

On the test system, **no available "mouse subsystem" implementation ever completes real
delegation to the pointer-draw routine.** Both VirtualBox's own Guest Additions mouse
driver (`VBoxMouse.sys`) and a third-party alternative (`AMOUSE.SYS`) call
`PTR_GETPTRDRAWADDRESS` purely as a zero-length capability probe, expect it to be
rejected, and then draw the cursor through their own internal mechanism regardless.
Answering that probe with a fabricated "success" instead of the documented rejection is
not just wrong — it crashed the caller outright in testing (a real trap inside
`VBOXMOUS`, which apparently assumes success means it received a genuine, usable
draw-routine address). The clone's own draw/erase/save-under logic is proven correct in
isolation (driven by a private test IOCtl, see below) — it simply was never exercised by
a real, live caller in any environment available to test against. Full details and raw
traces in `PLAN.md` §4a–§4c.

## How the protocol works

`POINTDD.SYS` registers as the device `POINTER$` and communicates with whatever calls it
("the mouse subsystem" — no released source exists showing exactly who this is) through
one documented IOCTL:

- **Category 3** (`IOCTL_SCR_AND_PTRDRAW`, `0x0003`), **Function `PTR_GETPTRDRAWADDRESS`
  (`0x72`)** — per IBM's own Physical Device Driver Reference, this is "used by the
  mouse subsystem to obtain the entry point address of the pointer draw routine ...
  supported by the physical Pointer Draw device driver." The physical driver answers
  this call; it never makes it. The response is a small packed struct:

  ```c
  typedef struct {
     USHORT usReturnCode;   /* 0 = success */
     PFN    pfnDraw;        /* 16:16 far pointer to the draw routine */
     char FAR16DATA *pchDataSeg;
  } PTRDRAWFUNCTION;
  ```

  A buffer shorter than `sizeof(PTRDRAWFUNCTION)` must be rejected (`RPERR_LENGTH`), not
  answered with a placeholder — see "Key finding" above for why that specifically
  matters in practice.

- The returned `pfnDraw` routine has the signature `void far draw_pointer(long ptlX, long
  ptlY)` (matching `pointer.asm`'s documented draw routine on the display-driver side),
  callable from both ring 0 and ring 3.

- For text modes, "drawing" means writing directly into the VGA text-mode
  character+attribute buffer (physical `0xB8000` for color modes) at the cursor's
  current character cell — saving the cell's original content first ("save-under") so it
  can be restored when the cursor moves or is removed.

This is *not* the same thing as Function `0x73` (`VID_INITCALLVECTOR`), a separate Call
Vector Table handshake used by display drivers — easy to conflate from the header
constants alone (both are Category 3), and this project initially did exactly that
before correcting course (see `PLAN.md` §2).

## Repository layout

```
src/                   MS C 6.0 + MASM + LINK build (Phase 1 reference, frozen)
src-wat/                OpenWatcom + Drv16Kit build, decoy device name "PTRSKEL$"
                         (the actively-developed build; safe to load alongside the
                         real POINTDD.SYS since nothing queries an unknown name)
src-wat-realname/        OpenWatcom + Drv16Kit build, REAL device name "POINTER$"
                         (swap-in test only - see "Testing" below)
test/                    Ring-3 test app (testptr.c) exercising the decoy build
                         directly via DosDevIOCtl
compile.cmd              Builds src/ (MS toolchain)
compile-wat.cmd          Builds src-wat/ (OpenWatcom, decoy name)
compile-realname.cmd     Builds src-wat-realname/ (OpenWatcom, REAL name)
compile-test.cmd         Builds test/testptr.exe
PLAN.md                  Full research/development log
docs/edm2-wiki-draft.txt Draft writeup for the EDM/2 wiki's POINTDD.SYS page
```

The `.cmd` scripts assume the project lives at a fixed path inside the build VM
(`D:\PROJECTS\DRIVERS\POINTDD`, via a VirtualBox shared folder) — update the hardcoded
paths near the top of each script if yours differs.

## Building

Two independent toolchains were proven; **OpenWatcom + Drv16Kit (`src-wat/`) is the
primary, actively-developed track** given its open-source-tooling advantage. `src/` (MS
C 6.0 + MASM + LINK) is kept as a working Phase 1 reference, not updated every phase.

### Prerequisites (not bundled in this repo)

- **[Drv16Kit](http://www.88watts.net/)** by David Azarewicz — supplies the 16-bit OS/2
  device-driver header/Strategy-dispatch boilerplate (`Drv16.lib`) so no hand-written
  MASM device header is needed. Required for `src-wat/` and `src-wat-realname/`.
- **A 16-bit OS/2 DDK** providing `devhelp.h`, `strategy.h`, `strat2.h`, `reqpkt.h`, and
  the matching import libraries (e.g. `os2286.lib`, `rmcalls.lib`, `dhcalls.lib`,
  `os2386.lib`) — e.g. IBM's own DDK, or the commonly-circulated "MiniDDK". Required for
  both toolchains; set your build environment's `INCLUDE`/`LIB` to point at it (see the
  `set` lines near the top of each `compile*.cmd`).
- **OpenWatcom** (`wcc`/`wlink`/`wmake`) for `src-wat/` and `src-wat-realname/`, or
  **MS C 6.0 + MASM + LINK** for `src/`.

### Build

Each `compile*.cmd` is self-logging (writes a matching `.log` file) and expects to run
on the OS/2 build VM itself, not the development host:

```
compile-wat.cmd          REM builds src-wat\PTRSKEL.sys
compile-realname.cmd     REM builds src-wat-realname\PDCLONE.sys (swap-in test only)
compile-test.cmd         REM builds test\testptr.exe
compile.cmd              REM builds src\PTRSKEL.OS2 (MS toolchain reference)
```

## Testing

**This is boot-time driver code loaded before the GUI exists — a bug can mean the
system doesn't boot at all.** Test in a VM, never on real hardware or your only working
install. Two distinct risk levels are used throughout development:

1. **Decoy-name testing** (`src-wat/`, device name `PTRSKEL$`): purely additive,
   loaded alongside the real, untouched `POINTDD.SYS`. Nothing in a real system queries
   an unknown device name, so this is low-risk — exercised directly via `test/testptr.c`
   calling `DosDevIOCtl` against it.

2. **Swap-in testing** (`src-wat-realname/`, device name `POINTER$`): this *replaces*
   the real driver's `CONFIG.SYS` line (`POINTER$` can only be claimed once), so it's a
   deliberate, temporary swap — never a lasting change. Procedure used throughout this
   project (see `PLAN.md` §4a for the full writeup):
   - Confirm your VM's recovery path (e.g. Alt-F1/maintenance partition) works, fresh,
     before starting.
   - Back up `CONFIG.SYS` under a distinct name.
   - Swap in the test driver's line, reboot, observe (a broken/invisible pointer is an
     acceptable outcome; a hang or trap is not).
   - **Swap back immediately after observing, regardless of outcome.**

Raw COM1 serial tracing (`com.log`) was used throughout for anything that needed
observing after boot completes — the kernel-message helper functions (`DevHelp_Save_
Message`, wrapped by `iprintf`) turned out to be boot-time-only despite documentation
that reads otherwise on a first pass; see `PLAN.md` for the full trap-and-fix writeup.
VirtualBox's serial-to-file capture appends across boots by default — clear `com.log` and
reboot fresh before trusting any single trace as representing one boot.

## Current limitations

- Verified on ArcaOS 5.1.2 under VirtualBox/VBoxVideo only — not yet tested on real
  hardware, Warp 4.52, or other display drivers.
- Draw/erase/save-under logic is proven correct in isolation (driven by a private test
  IOCtl under the decoy name), but never observed being invoked by a real, live caller —
  see "Key finding" above.
- No `MOU_*` (`IOCTL_POINTINGDEVICE`, category `0x07`) surface — that's `MOUSE$`'s own
  logical-driver interface, a different driver entirely, not something `POINTDD.SYS`
  implements.
- Graphics-mode drawing (the AND/XOR pointer-mask model `pointer.asm` documents) is not
  implemented — only text-mode VGA buffer writes.
- No session-switch/VDM handling yet.

## Acknowledgments

- IBM's own `PDDREF.pdf`, `DISPLAY.pdf`, and the `os2tk45` toolkit headers (`bsedev.h`)
  for the documented protocol this project builds against.
- David Azarewicz's [Drv16Kit](http://www.88watts.net/) for the OS/2 driver-building
  toolkit that made the OpenWatcom track possible.
- The [OS2World](https://www.os2world.com/) community.
- Developed with the assistance of Claude (Anthropic).

## License

BSD 3-Clause — see [`LICENSE`](LICENSE).
