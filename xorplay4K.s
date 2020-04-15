
; daveconv file.xr4 file.dav:e3,s

        org     00f0h
        defw    0500h, codeEnd - main, 0, 0, 0, 0, 0, 0

SFX_CHANNEL     equ     0a2h

musicData       equ     0800h
        assert  (musicData & 07ffh) == 0
envelopeData    equ     musicData
noteParamTable  equ     musicData + 1000h
trackOffsets    equ     musicData + 1800h

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
        ld      de, daveChannels
        ld      bc, 8
        ld      a, h
        add     a, high (noteParamTable - musicData)
        di
        ld      (npChn0), a
        ld      (npChn1), a
        ld      (npChn2), a
        ld      (npChn3), a
        add     a, high (trackOffsets - noteParamTable)
        ld      h, a
        ld      (toChn0), a
        ld      (toChn1), a
        ld      (toChn2), a
        ld      (toChn3), a
        sub     high (trackOffsets - musicData)
        rra
        ld      (edChn0), a
        ld      (edChn1), a
        ld      (edChn2), a
        ld      (edChn3), a
        xor     a
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

    macro daveChnPlay ch, fr, vr, lnp, led, lto
        ld      hl, ch
        dec     (hl)
        jr      nz, .l5
        ld      hl, (ch + 3)
.l1:    ld      a, (hl)
        inc     hl
        cp      1
        jr      nc, .l2
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
.l2:    ld      b, high noteParamTable  ; *
        jr      nz, .l9
        set     2, b
        ld      a, (hl)
        inc     hl
.l9:    ld      (ch + 3), hl
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
        ld      hl, (trackOffsets + (fr - 0a0h))        ; *
        jr      .l1
.l4:    ld      l, b
        ld      h, a
        and     1fh
        ld      (ch), a                         ; duration
        xor     h
        rlca
        rlca
        rlca                                    ; envelope H
        add     a, high (envelopeData >> 1)     ; *
        ld      h, a
        add     hl, hl
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
        inc     l
        out     (vr), a
        ld      a, (hl)
        out     (vr + 4), a
      if fr == SFX_CHANNEL
        defb    3eh                     ; = LD A, n
.l7:    inc     l
      else
        rla
      endif
        jr      c, .l8
        inc     hl
        ld      (ch + 1), hl
.l8:
lnp     equ     .l2 + 1
led     equ     .l4 + 12
lto     equ     .l3 + 5
      if fr == SFX_CHANNEL
sfxEnabled      equ     .l6
      endif
    endm

musicPlay:
        daveChnPlay daveChannels, 0a0h, 0a8h, npChn0, edChn0, toChn0
        daveChnPlay daveChannels + 5, 0a2h, 0a9h, npChn1, edChn1, toChn1
        daveChnPlay daveChannels + (2 * 5), 0a4h, 0aah, npChn2, edChn2, toChn2
        daveChnPlay daveChannels + (3 * 5), 0a6h, 0abh, npChn3, edChn3, toChn3
        ret

daveChannels:
        block   5 * 4, 00h

fileName:
        defb    0

        block   musicData - $, 00h

codeEnd:

