
; daveconv file.xr2 file.dav:e2,n,d,s

        org     00f0h
        defw    0500h, codeEnd - main, 0, 0, 0, 0, 0, 0

SFX_CHANNEL     equ     0a2h

musicData       equ     0800h
        assert  (musicData & 07ffh) == 0
envelopeData    equ     musicData
noteParamTable  equ     musicData + 0800h
trackOffsets    equ     musicData + 0c00h

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

; HL = base address (envelope data)

musicInit:
        ld      hl, trackOffsets
        ld      de, daveChannels
        ld      bc, 8
        xor     a
        di
        out     (0a7h), a
        inc     a
.l1:    ld      (de), a
        inc     de
        inc     de
        inc     de
        ldi
        ldi
        jp      pe, .l1
        ei
        ret

; Carry = 1: SFX enabled

enableSFX:
        sbc     a, a
        and     30h
        xor     28h
        ld      (sfxEnabled), a
        ret

    macro daveChnPlay ch, fr, vr
        ld      hl, ch
        dec     (hl)
        jr      nz, .l5
        ld      hl, (ch + 3)
.l1:    ld      a, (hl)
        inc     hl
        or      a
        jr      nz, .l2
        ld      a, (hl)
        inc     hl
        ld      b, (hl)
        inc     hl
        ld      e, (hl)
        inc     hl
    if fr != 0a6h
        ld      d, (hl)
        inc     hl
    endif
        ld      (ch + 3), hl
        jr      .l3
.l2:    ld      b, high noteParamTable
        ld      (ch + 3), hl
        ld      l, a
        ld      h, b
        ld      a, (hl)
        inc     h
        ld      b, (hl)
        inc     h
        ld      e, (hl)
      if fr != 0a6h
        inc     h
        ld      d, (hl)
      endif
.l3:    or      a
        jr      nz, .l4
        ld      hl, (trackOffsets + (fr - 0a0h))
        jr      .l1
.l4:    ld      l, b
        ld      h, a
        and     3fh
        ld      (ch), a                         ; duration
        xor     h
        rlca
        rlca                                    ; envelope H
        add     a, high envelopeData
        ld      h, a
      if fr != 0a6h
        ld      (.l6 - 2), de
      else
        ld      a, e
        ld      (.l6 - 1), a
      endif
        jr      .l6                     ; Carry = 0
.l5:    ld      hl, (ch + 1)
      if fr == SFX_CHANNEL
        ld      a, (hl)
        rla
      endif
      if fr != 0a6h
        ld      de, 0000h               ; * frequency
      else
        ld      a, 00h                  ; * channel 3 control
      endif
.l6:                                    ; Z = 0
      if fr == SFX_CHANNEL
        jr      z, .l7                  ; * 18h = FX enabled, 28h = FX disabled
      endif
      if fr != 0a6h
        ld      a, e
      endif
        out     (fr), a
      if fr != 0a6h
        ld      a, d
        out     (fr + 1), a
      endif
        ld      a, (hl)
        set     2, h
        out     (vr), a
        ld      a, (hl)
        out     (vr + 4), a
        res     2, h
      if fr == SFX_CHANNEL
.l7:
      else
        rla
      endif
        jr      c, .l8
        inc     hl
        ld      (ch + 1), hl
.l8:
      if fr == SFX_CHANNEL
sfxEnabled      equ     .l6
      endif
    endm

musicPlay:
        daveChnPlay daveChannels, 0a0h, 0a8h
        daveChnPlay daveChannels + 5, 0a2h, 0a9h
        daveChnPlay daveChannels + (2 * 5), 0a4h, 0aah
        daveChnPlay daveChannels + (3 * 5), 0a6h, 0abh
        ret

daveChannels:
        block   5 * 4, 00h

fileName:
        defb    0

        block   musicData - $, 00h

codeEnd:

