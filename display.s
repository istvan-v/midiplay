
LPT_ADDR        equ     0c000h
SCREEN_ADDR     equ     0d000h
INSNAME_ADDR    equ     0d800h
DRUMNAME_ADDR   equ     0ec00h

displayInit:
        ld      a, 0ffh
        out     (0b3h), a
        ld      hl, (0fff4h)
        set     7, h
        set     6, h
        set     2, l
        set     1, l
        ld      e, (hl)
        inc     l
        ld      d, (hl)
        ld      a, (videoSegment)
        out     (0b3h), a
        ld      hl, LPT_ADDR + (32 * 16)
        xor     a
        ld      b, a
.l1:    dec     hl
        ld      (hl), a
        dec     l
        ld      (hl), a
        djnz    .l1
        ld      bc, 1b23h
        ld      a, low SCREEN_ADDR
        call    .storeLPB
        ld      a, 0dbh
        ld      (bc), a
        call    .blankLPB
        ld      bc, 0400h + ((SCREEN_ADDR & 00ffh) + 8)
.l2:    push    bc
        push    de
        ld      a, (videoSegment)
        rrca
        rrca
        and     high (SCREEN_ADDR | 0c000h)
        ld      b, 0fdh
.l3:    ld      (hl), b
        inc     l
        ld      (hl), 08h
        inc     l
        ld      (hl), 14
        inc     l
        ld      (hl), 40h + 16
        inc     l
        ld      (hl), c
        inc     l
        ld      (hl), a
        inc     l
        ld      (hl), e
        inc     l
        ld      (hl), d
        inc     l
        inc     l
        ld      (hl), 3eh
        set     1, l
        ld      (hl), 49h
        set     2, l
        inc     hl
        inc     de
        inc     de
        bit     0, b
        jr      z, .l4
        inc     de
.l4:    sla     b
        set     2, b                    ; FDh, FEh, FCh
        jp      pe, .l3
        call    .blankLPB
        pop     de
        pop     bc
        ld      a, c
        add     a, 34
        ld      c, a
        djnz    .l2
        ld      bc, 136ah
        ld      a, low (SCREEN_ADDR + 8 + (34 * 4))
        call    .storeLPB
        ld      a, 38h
        ld      (bc), a
        set     1, c
        ld      a, 0c7h
        ld      (bc), a
        call    .blankLPB
        ld      b, 16
.l5:    push    bc
        ld      bc, 0d4fh
        ld      a, low (SCREEN_ADDR + 8)
        call    .storeLPB
        ld      a, c
        sub     low (LPT_ADDR + (29 * 16) + 9)
        cp      1
        ld      a, 74h
        rra
        ld      (bc), a
        set     1, c
        ld      a, 0e3h
        ld      (bc), a
        pop     bc
        djnz    .l5
        ld      e, l
        ld      d, h
        ld      hl, lptBorderVBlankBegin
        ld      bc, lptBorderVBlankSize
        ldir
        call    copyInstrNames
        ld      hl, programName
        ld      de, SCREEN_ADDR
        ld      bc, 8
        ldir
        ld      l, e
        ld      h, d
.l6:    ld      a, c
        or      0b0h
        ld      (hl), a
        inc     l
        ld      (hl), 20h
        inc     l
        ld      b, 26
.l7:    ld      (hl), 0eh
        inc     l
        djnz    .l7
        ld      b, 6
.l8:    ld      (hl), 8eh
        inc     l
        djnz    .l8
        inc     c
        bit     2, c
        jr      z, .l6
        ld      b, 23
.l9:    ld      (hl), 20h
        inc     l
        djnz    .l9
        ld      a, (page3Segment)
        out     (0b3h), a
        ret
.blankLPB:
        ld      bc, 3f00h
        xor     a
.storeLPB:
        ld      (hl), 256 - 9
        inc     l
        ld      (hl), 08h
        inc     l
        ld      (hl), b
        inc     l
        ld      (hl), c
        inc     l
        ld      (hl), a
        inc     l
        ld      a, (videoSegment)
        rrca
        rrca
        and     high (SCREEN_ADDR | 0c000h)
        ld      (hl), a
        inc     l
        ld      (hl), e
        inc     l
        ld      (hl), d
        inc     l
        inc     l
        ld      c, l
        ld      b, h
        set     2, l
        set     1, l
        inc     hl
        ret

copyInstrNames:
        ld      hl, instrumentNames
        ld      de, INSNAME_ADDR
        ld      bc, 26 * 256
.l1:    push    bc
        ld      b, 2
.l2:    ld      a, (hl)
        or      80h
        ld      (de), a
        inc     hl
        inc     de
        djnz    .l2
        pop     bc
        ld      a, 26 / 2
.l3:    ldi
        ldi
        dec     a
        jr      nz, .l3
        push    bc
        ld      a, 8eh
        ld      b, 12
.l4:    ld      (de), a
        inc     de
        djnz    .l4
        pop     bc
        ld      a, c
        or      b
        jr      nz, .l1
        ret

displayUpdate:
        ld      a, (videoSegment)
        out     (0b3h), a
        ld      hl, dave_regs + 8
        ld      de, LPT_ADDR + (2 * 16) + 3
        ld      bc, 043fh
.l1:    ld      a, (hl)
        and     c
        add     a, 0a1h
        rra
        ld      (de), a
        ld      a, e
        add     a, 32
        ld      e, a
        jr      nc, .l2
        inc     d
.l2:    bit     2, l
        set     2, l
        jr      z, .l1
        res     2, l
        inc     l
        djnz    .l1
        ld      hl, displayRMTable
        ld      b, 16
        ld      a, 69h
.l3:    ld      (hl), a
        inc     l
        djnz    .l3
        ld      hl, dave_midi_chn
        push    de
        ld      de, SCREEN_ADDR + 8 + (34 * 4)
        ld      bc, 087fh
        jr      .l5
.l4:    ld      a, ' '
        ld      (de), a
        inc     e
.l5:    ld      a, (hl)
        push    hl
        rla
        jr      c, .l6
        ld      a, '0' + 80h
        ld      (de), a
        inc     e
        ld      a, (hl)
        ld      hl, displayRMTable
        or      l
        ld      l, a
        inc     (hl)
        xor     low (displayRMTable ^ 00b0h)
        cp      0bah
        jr      c, .l8
        add     a, 'A' - ('9' + 1)
        jr      .l8
.l6:    ld      a, l
        and     DAVE_VIRT_CHNS - 1
        rla
        rla
        rla
        rla
        ld      hl, dave_chn
        or      l
        ld      l, a
        bit     7, (hl)
        jr      nz, .l7
        ld      a, (de)
        and     c                       ; C = 7Fh
        ld      (de), a
        inc     e
        ld      a, (de)
        and     c
        jr      .l8
.l7:    ld      a, '-'
        ld      (de), a
        inc     e
.l8:    ld      (de), a
        inc     e
        pop     hl
        inc     l
        djnz    .l4
        pop     hl
        ld      de, 32
        add     hl, de
        ld      de, midi_chn_program
        ld      bc, 1000h + (displayRMTable & 00ffh)
.l9:    ld      a, b
        sub     7
        cp      1
        ld      a, (de)
        rla
        rrca
        ex      de, hl
        push    hl
        ld      l, c
        ld      h, high displayRMTable
        ldi
        inc     bc
        push    de
        ld      e, a
        ld      d, 0
        ld      l, e
        ld      h, d
        add     hl, hl
        add     hl, hl
        add     hl, de
        add     hl, hl
        add     hl, hl
        add     hl, hl
        ld      de, INSNAME_ADDR
        ld      a, (videoSegment)
        rrca
        rrca
        and     d
        ld      d, a
        add     hl, de
        ex      de, hl
        pop     hl
        ld      (hl), e
        inc     l
        ld      (hl), d
        ld      de, 14
        add     hl, de
        pop     de
        inc     e
        inc     c
        djnz    .l9
        ld      a, (page3Segment)
        out     (0b3h), a
        ret

setPlayerLPT:
        ld      hl, LPT_ADDR | 0c000h
        ld      a, (videoSegment)
        rrca
        rrca
        and     h
        ld      h, a
        jr      setLPTAddress

restoreLPT:
        in      a, (0b3h)
        ld      b, a
        ld      a, 0ffh
        out     (0b3h), a
        ld      hl, (0fff4h)
        set     7, h
        set     6, h
        ld      a, b
        out     (0b3h), a

setLPTAddress:
        di
        ld      a, 1ch
.l1:    add     hl, hl
        rla
        jr      nc, .l1
        ld      c, 82h
        out     (c), h
        out     (83h), a
        ret

