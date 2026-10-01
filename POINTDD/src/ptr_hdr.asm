;***************************************************************************
; ptr_hdr.asm - POINTDD clone: device-driver header + segment glue (Phase 1)
;
; Modeled exactly on the WiFi RTL8188EU driver's rtl_hdr.asm (same DDK,
; same MS C 6.0 + MASM toolchain, same proven segment/header pattern).
;
; PHASE 1 SAFETY NOTE: the device name below is PTRSKEL$, a decoy - NOT the
; real POINTER$ name POINTDD.SYS registers. This keeps Phase 1 purely
; additive: it loads alongside the real, untouched POINTDD.SYS/MOUSE.SYS and
; cannot conflict with or interfere with them. Do not change this to
; POINTER$ until Phase 1 has proven the toolchain + load-safety loop on the
; VM (see ..\PLAN.md Phase 1 / Phase 4 safety notes). Drop the DEV_30 and
; DEV_IOCTL bits stay unset too - Phase 1 does nothing beyond Init/
; InitComplete, so nothing else needs to be requested from the kernel.
;***************************************************************************
        .XCREF
        .XLIST
        INCLUDE devhdr.inc
        .LIST
        .CREF

        PUBLIC  _gHead
        EXTRN   _Strategy:FAR

;--- Device driver header (must be first in DGROUP) -----------------------
HEADER  SEGMENT WORD PUBLIC 'HEADER'
        EVEN
_gHead  LABEL   WORD
        dd      -1                                      ; far ptr to next header
        dw      DEV_CHAR_DEV OR DEVLEV_3                ; attribute word
        dw      OFFSET _Strategy                        ; strategy routine offset
        dw      0                                       ; no IDC entry (Phase 1)
        db      "PTRSKEL$"                              ; 8-char driver name (decoy)
        dw      0                                       ; prot-mode CS selector
        dw      0                                       ; prot-mode DS selector
        dw      0                                       ; real-mode CS segment
        dw      0                                       ; real-mode DS segment
        dd      DEV_INITCOMPLETE                        ; capabilities strip
HEADER  ENDS

;--- declare all remaining segments (empty) before grouping ---------------
_DATA   segment word public 'DATA'
_DATA   ends
CONST   segment word public 'CONST'
CONST   ends
_BSS    segment word public 'BSS'
_BSS    ends
RMCode  segment word public 'CODE'
RMCode  ends
Code    segment word public 'CODE'
Code    ends
_TEXT   segment word public 'CODE'
_TEXT   ends

;--- groups (all member segments now exist) -------------------------------
DGROUP  GROUP   HEADER, CONST, _BSS, _DATA
CGROUP  GROUP   RMCode, Code, _TEXT

        END
