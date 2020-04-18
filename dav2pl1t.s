
; daveconv2 file.dav file.bin 0x1000:s

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
noteParamTables equ     musicData
trackOffsets    equ     noteParamTables + 0300h

musicInit:
        xor     a
        di
.l1:    add     a, a                    ; A = 80h on restart
        out     (0a7h), a
        ld      (musicPlay.lF0 + 1), a
        ld      (musicPlay.lV0 + 1), a
        ld      (musicPlay.lF1 + 1), a
        ld      (musicPlay.lV1 + 1), a
        ld      (musicPlay.lF2 + 1), a
        ld      (musicPlay.lV2 + 1), a
        ld      (musicPlay.lF3 + 1), a
        ld      (musicPlay.lV3 + 1), a
        ld      hl, (trackOffsets)
        ld      (musicPlay.ltptr + 1), hl
;       ei
        jr      c, musicPlay.ltptr      ; restart?
        ret

  macro daveChnRegsPlay reg0, reg1
.l1:    ld      a, 0                    ; * envelope timer * 2
        add     a, a
        jr      nz, .l4
        dec     sp
        pop     af
        add     a, a
        jr      c, .l2                  ; using parameter table?
        pop     hl
    if reg0 == 0a6h
        jp      m, .l3
        dec     sp                      ; channel 3 frequency: no MSB
    endif
        jr      .l3
.l2:
    if reg0 < 0a8h
        rrca
    else
        rra
    endif
        ld      l, a
        ld      h, b                    ; B = high noteParamTables
        ld      a, (hl)
        inc     h
        ld      e, (hl)
        inc     h
        ld      h, (hl)
        ld      l, e                    ; A < 80h: literal/RLE,
.l3:    cp      c                       ; A > 80h: use table (C = 80h)
    if reg0 == 0a0h
        jr      z, musicInit.l1         ; end of track?
    endif
        jr      nc, .l6
        ld      (.l5 + 1), hl
        jr      .l7
.l4:    adc     a, d                    ; D = -4
        rrca
.l5:    ld      hl, 0000h               ; * envelope data pointer
        jr      nc, .l7
        inc     l
        inc     hl
.l6:    ld      (.l5 + 1), hl
    if reg0 != 0a6h
        ld      e, (hl)
        inc     l
        ld      h, (hl)
        ld      l, e
    else
        ld      l, (hl)
    endif
.l7:    ld      (.l1 + 1), a            ; store (duration - 1) * 2
        ld      a, l
        out     (reg0), a
    if reg0 != 0a6h
        ld      a, h
        out     (reg1), a
    endif
  endm

musicPlay:
        ld      bc, noteParamTables + 0080h
        ld      d, 256 - 4
        di
        ld      (.lSP + 1), sp
.ltptr: ld      sp, trackOffsets + 2    ; * track data pointer
.lF0:   daveChnRegsPlay 0a0h, 0a1h
.lV0:   daveChnRegsPlay 0a8h, 0ach
.lF1:   daveChnRegsPlay 0a2h, 0a3h
.lV1:   daveChnRegsPlay 0a9h, 0adh
.lF2:   daveChnRegsPlay 0a4h, 0a5h
.lV2:   daveChnRegsPlay 0aah, 0aeh
.lF3:   daveChnRegsPlay 0a6h, 0a7h
.lV3:   daveChnRegsPlay 0abh, 0afh
        ld      (.ltptr + 1), sp
.lSP:   ld      sp, 0000h               ; *
        ret

; -----------------------------------------------------------------------------

fileName:
        defb    0

        block   musicData - $, 00h

codeEnd:

