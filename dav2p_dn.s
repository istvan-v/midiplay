
; daveconv2 file.dav file.bin 0x1000:dn

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
        call    musicInit1
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
noteParamTables equ     musicData
trackOffsets    equ     noteParamTables + 0300h

musicInit1:
        ld      hl, (trackOffsets)
        ld      de, 10000h - (trackOffsets + 16)
        add     hl, de
        srl     h
        rr      l
        ld      (musicPlay.l1 + 1), hl
        ld      c, l
        ld      b, h
        ld      a, l
        or      h
        jr      z, musicInit
        xor     a
        ld      hl, trackOffsets + 16
.l1:    add     a, (hl)                 ; decode differential format
        ld      (hl), a
        inc     l
        add     a, (hl)
        ld      (hl), a
        cpi
        jp      pe, .l1

musicInit:
        xor     a
.l1:    di                              ; A = 80h on restart
        add     a, a
        out     (0a7h), a
        ld      (envTimerF0), a
        ld      (envTimerV0), a
        ld      (envTimerF1), a
        ld      (envTimerV1), a
        ld      (envTimerF2), a
        ld      (envTimerV2), a
        ld      (envTimerF3), a
        ld      (envTimerV3), a
        ld      (.l2 + 1), sp
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
.l2:    ld      sp, 0000h               ; *
;       ei
        ret     nc                      ; not restart?

  macro daveChnRegsPlay reg0, reg1, lenvt, ltptr
.l1:    ld      a, 0                    ; * envelope timer * 2
        add     a, a
        jr      nz, .l5
.l2:    ld      hl, 0000h               ; * track data pointer
        ld      a, (hl)
        inc     hl
        add     a, a
        jr      c, .l3                  ; using parameter table?
        ld      e, (hl)
        inc     hl
    if reg0 == 0a6h
        jp      p, .l3                  ; channel 3 frequency: no MSB
    endif
        ld      d, (hl)
        inc     hl
.l3:    ld      (.l2 + 1), hl
        jr      nc, .l4
    if reg0 < 0a8h
        rrca
    else
        rra
    endif
        ld      l, a
        ld      h, high noteParamTables
        ld      a, (hl)
        inc     h
        ld      e, (hl)
        inc     h
        ld      d, (hl)
.l4:    ex      de, hl                  ; A < 80h: literal/RLE,
        cp      80h                     ; A > 80h: use table
    if reg0 == 0a0h
        jr      z, musicInit.l1         ; end of track?
    endif
        jr      nc, .l7
        ld      (.l6 + 1), hl
        jr      .l8
.l5:    adc     a, 256 - 4
        rrca
.l6:    ld      hl, 0000h               ; * envelope data pointer
        jr      nc, .l8
        inc     hl
.l7:    ld      (.l6 + 1), hl
    if reg0 != 0a6h
        ld      e, (hl)
        add     hl, bc                  ; BC = envelope data size / 2
        ld      h, (hl)
        ld      l, e
    else
        ld      l, (hl)
    endif
.l8:    ld      (.l1 + 1), a            ; store (duration - 1) * 2
        ld      a, l
        out     (reg0), a
    if reg0 != 0a6h
        ld      a, h
        out     (reg1), a
    endif

lenvt   equ     .l1 + 1
ltptr   equ     .l2 + 1
  endm

musicPlay:
.l1:    ld      bc, 0                   ; * envelope data size / 2
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

