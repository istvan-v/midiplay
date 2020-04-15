
; daveconv2 file.dav file.bin 0x1000

        org     00f0h
        defw    0500h, codeEnd - main, 0, 0, 0, 0, 0, 0

    macro exos n
        rst     30h
        defb    n
    endm

main:
        di
        ld      sp, 0100h
        ld      a, 0ffh
        out     (0b2h), a
        ld      hl, resetRoutine
        ld      (0bff8h), hl
        ei
        exos    24
        jr      nz, resetRoutine
        ld      a, c
        out     (0b1h), a
        exos    24
        jr      nz, resetRoutine
        ld      a, c
        out     (0b2h), a
        ld      bc, 011bh
        ld      d, 00h
        exos    16
        ld      bc, 011ch
        ld      d, 00h
        exos    16
        halt
        halt
        ld      de, fileName
        xor     a
        exos    1
        jr      nz, resetRoutine
        ld      de, musicData
        ld      bc, 0c000h - musicData
        exos    6
        xor     0e4h                    ; .EOF
        jr      nz, resetRoutine
        exos    3
        di
        ld      a, 0ch
        out     (0bfh), a
        ld      a, 30h
        out     (0b4h), a
        ld      a, 0c3h                 ; = JP nn
        ld      (0038h), a
        ld      hl, irqRoutine
        ld      (0039h), hl
        ld      hl, musicData
        call    musicInit
        ei
.l1:    jr      .l1

irqRoutine:
        push    af
        ld      a, 30h
        out     (0b4h), a
        push    bc
        push    de
        push    hl
        call    musicPlay
        pop     hl
        pop     de
        pop     bc
        pop     af
        ei
        ret

resetRoutine:
        di
        ld      sp, 3800h
        ld      a, 0ffh
        out     (0b2h), a
        ld      hl, resetRoutine
        ld      (0bff8h), hl
        ld      c, 40h
        exos    0
        ld      a, 01h
        out     (0b3h), a
        ld      a, 6
        jp      0c00dh

; -----------------------------------------------------------------------------

musicData       equ     1000h
        assert  (musicData & 00ffh) == 0
noteParamTableF equ     musicData
noteParamTableV equ     noteParamTableF + 0200h
trackOffsets    equ     noteParamTableV + 0100h

musicInit:
        xor     a
        di
        out     (0a7h), a
        ld      (envTimerF0), a
        ld      (envTimerV0), a
        ld      (envTimerF1), a
        ld      (envTimerV1), a
        ld      (envTimerF2), a
        ld      (envTimerV2), a
        ld      (envTimerF3), a
        ld      (envTimerV3), a
        ld      (.l1 + 1), sp
        ld      sp, trackOffsets
        pop     hl
        ld      (trackPtrF0), hl
        pop     hl
        ld      (trackPtrF1), hl
        pop     hl
        ld      (trackPtrF2), hl
        pop     hl
        ld      (trackPtrF3), hl
        pop     hl
        ld      (trackPtrV0), hl
        pop     hl
        ld      (trackPtrV1), hl
        pop     hl
        ld      (trackPtrV2), hl
        pop     hl
        ld      (trackPtrV3), hl
.l1:    ld      sp, 0000h               ; *
;       ei
        ret

  macro daveChnRegsPlay reg0, reg1, lenvt, ltptr
.l1:    ld      a, 0                    ; * envelope timer * 4
        sub     4
        jr      nc, .l4
.l2:    ld      hl, 0000h               ; * track data pointer
        ld      a, (hl)
        inc     hl
        add     a, a
        jr      c, .l6                  ; using parameter table?
        ld      e, (hl)
        inc     hl
    if reg0 == 0a6h
        jp      p, .l3                  ; channel 3 frequency: no MSB
    endif
        ld      d, (hl)
        inc     hl
.l3:    ld      (.l2 + 1), hl
        jr      .l7
.l4:    ld      (.l1 + 1), a
.l5:    ld      hl, 0000h               ; * envelope data pointer
        inc     l
        inc     hl
        jr      .l9
.l6:    ld      (.l2 + 1), hl
        ld      l, a
    if reg0 < 0a8h
        ld      h, high noteParamTableF + 1
        ld      a, (hl)
        dec     h
        ld      e, (hl)
        inc     l
        ld      d, (hl)
    else
        ld      h, high noteParamTableV
        ld      e, (hl)
        inc     l
        ld      d, (hl)
        dec     h
        ld      a, (hl)
    endif
.l7:    add     a, a
    if reg0 == 0a0h
        jr      nz, .l8
        jr      c, musicRestart         ; end of track?
    endif
.l8:    ld      (.l1 + 1), a            ; store (duration - 1) * 4
        ccf
        sbc     a, a                    ; 00h: use table, 1FFh: literal/RLE
        add     a, 22h                  ; 21h: LD HL, nn
        ld      (.l9), a                ; 22h: LD (nn), HL
        ld      l, e
        ld      h, d
        ld      (.l5 + 1), hl
.l9:    ld      (.l5 + 1), hl           ; *
        ld      a, (hl)
    if reg0 != 0a6h
        inc     l
    endif
        out     (reg0), a
    if reg0 != 0a6h
        ld      a, (hl)
        out     (reg1), a
    endif

lenvt   equ     .l1 + 1
ltptr   equ     .l2 + 1
  endm

musicRestart:
        call    musicInit

musicPlay:
        daveChnRegsPlay 0a0h, 0a1h, envTimerF0, trackPtrF0
        daveChnRegsPlay 0a8h, 0ach, envTimerV0, trackPtrV0
        daveChnRegsPlay 0a2h, 0a3h, envTimerF1, trackPtrF1
        daveChnRegsPlay 0a9h, 0adh, envTimerV1, trackPtrV1
        daveChnRegsPlay 0a4h, 0a5h, envTimerF2, trackPtrF2
        daveChnRegsPlay 0aah, 0aeh, envTimerV2, trackPtrV2
        daveChnRegsPlay 0a6h, 0a7h, envTimerF3, trackPtrF3
        daveChnRegsPlay 0abh, 0afh, envTimerV3, trackPtrV3
        ret

; -----------------------------------------------------------------------------

fileName:
        defb    0

        block   musicData - $, 00h

codeEnd:

