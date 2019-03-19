
ENABLE_CHN1_ALLOC       equ     1
ENABLE_VELOCITY         equ     1
;PANNED_NOTE_NEW        equ     1

; DE = round(DE * HL / 10000h)

dave_oct_mult:
        push    bc
        ld      a, 16
        ld      c, l
        ld      b, h
        ld      hl, 0
.l1:    add     hl, hl
        rl      e
        rl      d
        jr      nc, .l2
        add     hl, bc
        jr      nc, .l2
        inc     de
.l2:    dec     a
        jp      nz, .l1
        pop     bc
        or      h
        ret     p
        inc     de
        ret

dave_init:
        ld      hl, oct_table + 05ffh
        ld      de, oct_table + 05feh
        ld      bc, 05ffh
        ld      (hl), 00h
        lddr
        ld      (hl), low 34323
        inc     l
        ld      (hl), high 34323
        ld      bc, 512
        ld      de, freq_mult_table
.l1:    ld      hl, oct_table + 1
        add     hl, bc
.l2:    ld      a, (hl)
        or      a
        jr      nz, .l3
        dec     l
        push    de
        push    hl
        sbc     hl, bc
        ld      a, (hl)
        inc     l
        ld      h, (hl)
        ld      l, a
        ex      de, hl
        ld      a, (hl)
        inc     l
        ld      h, (hl)
        ld      l, a
        ex      de, hl
        call    dave_oct_mult
        pop     hl
        ld      (hl), e
        inc     l
        ld      (hl), d
        pop     de
.l3:    add     hl, bc
        ld      a, h
        cp      high (oct_table + 0600h)
        jr      c, .l2
        inc     de
        inc     de
        srl     b
        rr      c
        bit     1, c
        jr      z, .l1
        ld      hl, oct_table + 05fdh
        ld      de, oct_table + 05fbh
.l4:    ld      b, (hl)
        dec     l
        ld      a, (hl)
        dec     hl
        dec     l
        dec     hl
        dec     l
        add     a, (hl)
        ld      c, a
        inc     l
        ld      a, b
        adc     a, (hl)
        rra
        ld      b, a
        rr      c
        jr      nc, .l5
        inc     bc
.l5:    ld      a, b
        ld      (de), a
        dec     e
        ld      a, c
        ld      (de), a
        dec     de
        dec     e
        dec     de
        ld      a, d
        cp      high oct_table
        jr      nc, .l4
        ld      hl, (oct_table + 05fch)
        ld      de, (freq_mult_table + (8 * 2))
        call    dave_oct_mult
        ld      (oct_table + 05feh), de
        ld      hl, 0
        ld      de, 284
        ld      bc, sin_table
.l6:    ld      a, l
        rla
        ld      a, h
        adc     a, 0
        ld      (bc), a
        add     hl, de
        push    hl
        xor     a
        add     hl, hl
        rla
        add     hl, hl
        rla
        ld      l, h
        ld      h, a
        push    de
        ld      e, l
        ld      d, h
        add     hl, hl
        add     hl, hl
        add     hl, de
        add     hl, hl
        add     hl, hl
        add     hl, hl
        add     hl, de
        ld      de, 8258
        add     hl, de
        pop     de
        ld      a, h
        rlca
        rlca
        and     03h
        ld      l, a
        ld      a, e
        sub     l
        ld      e, a
        sbc     a, a
        add     a, d
        ld      d, a
        pop     hl
        inc     c
        jr      nz, .l6

dave_reset:
        ld      hl, dave_regs
        ld      de, dave_regs + 1
        ld      bc, 15
        ld      (hl), b
        ldir
        ld      hl, midi_dave_chn
        ld      de, midi_dave_chn + 1
        ld      c, 15
        ld      (hl), b
        ldir
        ld      hl, dave_chn + (DAVE_VIRT_CHNS * 16 - 1)
        ld      de, dave_chn + (DAVE_VIRT_CHNS * 16 - 2)
        ld      c, DAVE_VIRT_CHNS * 16 - 1
        ld      (hl), b
        lddr
        ld      de, 7f80h
.l1:    ld      (hl), 0a0h              ; env_state
        set     3, l
        dec     l
        ld      (hl), e                 ; pitch
        inc     l
        inc     l
        ld      (hl), d                 ; veloc
        set     1, l
        ld      (hl), 64                ; pan
        inc     l
        ld      (hl), d                 ; vol
        inc     l
        ld      (hl), e                 ; vol_l
        inc     l
        ld      (hl), e                 ; vol_r
        inc     l
        inc     hl
        ld      a, l
        xor     low (dave_chn + (DAVE_VIRT_CHNS * 16))
        jr      nz, .l1
        ld      (dave_chn0_index), a
        inc     a
        ld      (dave_chn1_index), a
        ret

; A = DAVE channel (0..7), n = structure member offset (0..15)
; sets HL = pointer to DaveChannel structure

    macro dave_channel_ptr  n
        add     a, a
        add     a, a
        add     a, a
        add     a, a
        ld      hl, dave_chn + n
        or      l
        ld      l, a
    endm

; A = DAVE channel (0..7)
; B = key (0 to 127)

dave_channel_release:
        dave_channel_ptr  0
.l1:    bit     4, (hl)
        jr      z, .l3
        ld      (hl), 60h               ; release
        inc     l
        ld      e, (hl)
        inc     l
        ld      d, (hl)                 ; DE = chn->env_ptr
        ld      a, (de)
        cp      0c0h
        jr      c, .l2
        inc     e
        inc     de
        inc     e
        inc     de
        dec     l
        ld      (hl), e
        inc     l
        ld      (hl), d
.l2:    set     2, l                    ; HL = &(chn->env_timer) + 1
        xor     a
        ld      (hl), a
        dec     l
        ld      (hl), a
        ret
.l3:    ld      a, l
        cp      low (dave_chn + (3 * 16))
        jr      nz, dave_channel_off.l1
        ret

; A = DAVE channel (0..7)

dave_channel_off:
        dave_channel_ptr  0
.l1:    ld      (hl), 0a0h              ; chn->env_state
        set     2, l
        inc     l
        xor     a
        ld      (hl), a                 ; chn->env_timer
        inc     l
        ld      (hl), a
        ret

; B = DAVE channel 0 (0..7) * 16
; A = DAVE channel 1 (0..7) * 16
; C = DAVE channel 2 (0..7) * 16
; returns IX = DAVE channel structure address

find_best_channel:
        ld      hl, dave_chn
        or      l
        ld      l, a                    ; HL = chn1
        ld      a, b                    ; c0 * 16
        or      low dave_chn
        ld      ixl, a
        ld      ixh, high dave_chn      ; IX = chn0
        ld      b, (ix)                 ; chn0->env_state
        res     4, b                    ; B = chn0->env_state & 0xEF
        ld      a, (hl)                 ; chn1->env_state
        and     0efh
        cp      b
        jr      c, .l2
        jr      nz, .l1
        set     2, l
        inc     l                       ; HL = &(chn1->env_timer)
        ld      a, (ix + 5)
        cp      (hl)
        inc     l
        ld      a, (ix + 6)
        sbc     a, (hl)
        jr      nc, .l2                 ; chn0->env_timer >= chn1->env_timer?
.l1:    ld      a, l
        and     0f0h
        ld      ixl, a
        ld      l, a
        ld      b, (hl)
        res     4, b                    ; B = chn1->env_state & 0xEF
.l2:    ld      a, c                    ; c2 * 16
        or      low dave_chn
        ld      l, a                    ; HL = chn2
        ld      a, (hl)                 ; chn2->env_state
        and     0efh
        cp      b
        ret     c
        ld      b, l
        jr      nz, .l3
        set     2, l
        inc     l                       ; HL = &(chn2->env_timer)
        ld      a, (ix + 5)
        cp      (hl)
        inc     l
        ld      a, (ix + 6)
        sbc     a, (hl)
        ret     nc                      ; chn0->env_timer >= chn2->env_timer?
.l3:    ld      ixl, b
        ret

; A = midi_chn (0 to 15)
; B = pgm
; C = veloc
; DE = pitch
; HL = ctrls
;
; returns A = DAVE channel

dave_channel_on:
        push    ix
        push    hl
        cp      9
        jr      nz, .l1
        ld      ix, dave_chn + (3 * 16) ; percussion
        ld      l, d
        ld      h, high drum_env_offsets
        jr      .l6
.l1:    ld      hl, midi_dave_chn
        or      l
        ld      l, a
        ld      a, (hl)
        or      a
        jr      z, .l3                  ; dynamic channel allocation?
        rla
        rla
        rla
        rla
        cp      40h
        jr      nc, .l2
        add     a, low (dave_chn - 16)  ; simple fixed channel
        ld      ixl, a
        ld      ixh, high dave_chn
        jr      .l5
.l2:    push    bc
        and     10h
        ld      b, a
        or      60h
        ld      c, a
        jr      .l4
.l3:    push    bc
        ld      a, l
        and     01h
        rrca
        rrca
        rrca
        ld      b, a
    if ENABLE_CHN1_ALLOC == 0
        ld      c, a
    else
        ld      c, 10h
    endif
.l4:    xor     20h
        call    find_best_channel
        pop     bc
.l5:    ld      l, b
        ld      h, high pgm_env_offsets
.l6:    ld      (ix + 7), e             ; chn->pitch
        ld      (ix + 8), d
    if ENABLE_VELOCITY == 0
        ld      (ix + 9), 127           ; chn->veloc
    else
        ld      (ix + 9), c
    endif
        sla     l
        ld      a, (hl)
        add     a, a
        ld      (ix + 1), a             ; chn->env_ptr
        inc     l
        ld      a, (hl)
        ld      c, a
        rla
        and     1fh
        add     a, high envelope_data
        ld      (ix + 2), a
        ld      a, c
        and     0b0h
        ld      (ix), a                 ; chn->env_state
        pop     hl                      ; HL = ctrls
        ld      a, (hl)
        add     a, a
        add     a, a
        ld      (ix + 10), a            ; chn->dist
        set     1, l
        ld      a, (hl)
        ld      (ix + 11), a            ; chn->pan
        inc     l
        ld      a, (hl)
        ld      (ix + 12), a            ; chn->vol
        ld      (ix + 13), 0ffh         ; chn->vol_l
        ld      (ix + 15), 0            ; chn->aftertouch
        ld      (ix + 5), 0             ; chn->env_timer
        ld      (ix + 6), 0
        ex      (sp), ix
        pop     hl                      ; HL = chn
        ld      a, l
        and     16 * (DAVE_VIRT_CHNS - 1)
        rra
        rra
        rra
        rra
        bit     6, c
        ret     z                       ; not using panned instrument?
        cp      3
        ret     z

; HL = chn
; DE = pitch

pan_note:
        ld      b, a
        ld      a, l
        or      11
        ld      l, a                    ; HL = &(chn->pan)
    if PANNED_NOTE_NEW == 0
        ld      a, (hl)
        push    hl
        ld      l, d
        ld      h, 0
        add     hl, hl
        add     hl, hl
        add     a, l
        ld      l, a
        adc     a, h
        scf
        sbc     a, l
        ld      a, l
        pop     hl
        jp      m, .l1
        jr      nz, .l2
        cp      low 380
        jr      nc, .l2
        inc     a
        ld      (hl), a
        ld      a, b
        ret
.l1:    ld      (hl), 1
        ld      a, b
        ret
.l2:    ld      (hl), 125
    else
        ld      a, d
        rla
        add     a, (hl)
        add     a, a
        add     a, 193
        jp      p, .l1
        cpl
.l1:    ld      (hl), a
    endif
        ld      a, b
        ret

; A = DAVE channel
; B = value

dave_chn_distortion:
        dave_channel_ptr  10
        ld      a, b
        add     a, a
        add     a, a
        ld      (hl), a                 ; chn->dist
        ret

; A = MIDI channel (0 to 15)
; B = controller value

dave_assign_channel:
        ld      hl, midi_dave_chn
        or      l
        ld      l, a
        ld      a, b
        rra
        rra
        and     07h
        ld      (hl), a
        ret

; A = DAVE channel
; B = value

dave_chn_set_pan:
        dave_channel_ptr  11
        ld      (hl), b                 ; chn->pan
        inc     l
        inc     l
        ld      (hl), 0ffh              ; chn->vol_l
        ret

; A = DAVE channel
; B = value

dave_chn_set_volume:
        dave_channel_ptr  12
        ld      (hl), b                 ; chn->vol
        inc     l
        ld      (hl), 0ffh              ; chn->vol_l
        ret

; A = DAVE channel
; B = value

dave_chn_aftertouch:
        dave_channel_ptr  15
        ld      a, b
        sub     (hl)                    ; chn->aftertouch
        ld      (hl), b
        res     1, l
        ld      (hl), 0ffh              ; chn->vol_l
        res     2, l
        add     a, (hl)                 ; chn->veloc
        ld      (hl), a
        ret

; returns L = (H * L + 64) / 128, limited to the range 0 to 128

volume_mult:
        ld      e, l
        ld      d, 0
        sla     h
        jr      c, .l1
        ld      l, d
.l1:    add     hl, hl
        inc     l
        jr      nc, .l2
        add     hl, de
.l2:    add     hl, hl
        jr      nc, .l3
        add     hl, de
.l3:    add     hl, hl
        jr      nc, .l4
        add     hl, de
.l4:    add     hl, hl
        jr      nc, .l5
        add     hl, de
.l5:    add     hl, hl
        jr      nc, .l6
        add     hl, de
.l6:    add     hl, hl
        jr      nc, .l7
        add     hl, de
.l7:    add     hl, hl
        jr      nc, .l8
        add     hl, de
.l8:    add     hl, hl
        ld      l, h
        rra
        or      l
        ret     p
        ld      l, 80h
        ret

; A = DAVE channel (0 to 7)
; B = pitch bend >> 6

dave_channel_pitch:
        dave_channel_ptr  7
        ld      (hl), b                 ; chn->pitch
        ret

; converts MIDI pitch to DAVE frequency in HL

    macro pitch_to_dave_freq
        ld      a, h
        sub     high 1800h
        jr      c, .l2
        ld      b, 8
        jp      p, .l3
.l1:    ld      hl, 0fffh
        jr      .l11
.l2:    ld      a, l
        cp      low 1588
        ld      a, h
        sbc     a, high 1588
        jr      c, .l1
        ld      b, 0
        ld      a, h
.l3:    cp      high 0c00h
        jr      c, .l4
        sub     high 0c00h
        set     2, b
.l4:    cp      high 0600h
        jr      c, .l5
        sub     high 0600h
        set     1, b
.l5:    cp      high 0300h
        jr      c, .l6
        sub     high 0300h
        inc     b
.l6:    sla     l
        rla
        add     a, high oct_table
        ld      h, a
        ld      a, (hl)
        inc     l
        ld      h, (hl)
        bit     3, b
        jr      z, .l7
        ld      a, h
        ld      h, 0
.l7:    bit     2, b
        jr      z, .l8
        srl     h
        rra
        srl     h
        rra
        srl     h
        rra
        srl     h
        rra
.l8:    bit     1, b
        jr      z, .l9
        srl     h
        rra
        srl     h
        rra
.l9:    bit     0, b
        jr      z, .l10
        srl     h
        rra
.l10:   ld      l, a
        dec     hl
        srl     h
        rr      l
.l11:
    endm

dave_chn3_dist:
        ld      a, (ix + 10)            ; chn->dist
        xor     b                       ; B = envelope distortion
        ld      l, a
        ld      h, 0
        ret

; IX = chn
; BC = (distortion << 8) | pitch_bend
;
; returns HL = DAVE frequency

dave_chn_calc_freq:
        ld      a, ixl
        cp      low (dave_chn + (3 * 16))
        jr      z, dave_chn3_dist       ; channel 3?
        ld      l, (ix + 7)
        ld      h, (ix + 8)             ; HL = chn->pitch
        xor     a
        srl     h
        rra
        srl     h
        rra
        add     a, l
        ld      l, a
        jr      nc, .l1
        inc     h
.l1:    ld      d, b
        ld      a, b
        add     a, high 0800h
        and     high 0fffh
        sub     high 0800h
        ld      b, a
        add     hl, bc
        pitch_to_dave_freq
        ld      a, d
        xor     (ix + 10)
        and     0f0h                    ; A = envelope distortion ^ chn->dist
        or      h
        ld      h, a
        and     30h
        ret     pe                      ; not 4 or 5 bit distortion?
        and     20h
        jr      nz, .l6                 ; 5-bit distortion?
        ld      e, l
        ld      a, h
        and     0fh
        ld      d, a
        jr      z, .l3
.l2:    add     a, e
        ld      e, a
        adc     a, 0
        sub     e
        ld      d, a
        jr      nz, .l2
.l3:    ld      a, e
        and     0f0h
        jr      z, .l5
.l4:    rra
        rra
        rra
        rra
        ld      d, a
        ld      a, e
        and     0fh
        add     a, d
        ld      e, a
        and     0f0h
        jr      nz, .l4
.l5:    ld      a, (ix + 10)            ; chn->dist
        and     08h
        rla
        ld      bc, poly4_offs_table_15
        assert  poly4_offs_table_5 == (poly4_offs_table_15 | 0010h)
        or      c
        or      e
        ld      c, a
        ld      a, (bc)
        ld      e, a
        rla
        sbc     a, a
        ld      d, a
        add     hl, de
        ret
.l6:    ld      e, l
        ld      a, h
        and     0fh
        ld      d, a
.l7:    ld      a, e
        and     0e0h
        or      d
        jr      z, .l8
        ld      a, e
        ld      c, d
        ld      b, 0
        rla
        rl      c
        rl      b
        rla
        rl      c
        rl      b
        rla
        rl      c
        rl      b
        ld      a, e
        and     1fh
        add     a, c
        ld      e, a
        adc     a, b
        sub     e
        ld      d, a
        jp      .l7
.l8:    ld      a, e
        cp      30
        ret     nz
        inc     hl
        ret

set_dave_registers:
        ld      hl, dave_regs
        ld      bc, 109fh
.l1:    inc     c
        outi
        jp      nz, .l1
        ret

update_chn_01_index:
        ld      a, (dave_chn)
        ld      hl, dave_chn + (4 * 16)
        and     (hl)
        ld      l, low (dave_chn + (6 * 16))
        and     (hl)
        ld      hl, dave_chn0_index
        ld      c, 0
        jp      m, .l2
        ld      c, (hl)
.l1:    ld      de, chn_index_table
        ld      a, c
        or      e
        ld      e, a
        ld      a, (de)
        ld      c, a
        rla
        rla
        rla
        rla
        ld      de, dave_chn
        or      e
        ld      e, a
        ld      a, (de)
        rla
        jr      c, .l1
.l2:    ld      (hl), c
        ld      a, (dave_chn + 16)
        ld      hl, dave_chn + (5 * 16)
        and     (hl)
        ld      l, low (dave_chn + (7 * 16))
        and     (hl)
        ld      hl, dave_chn1_index
        ld      c, (hl)
        ld      (hl), 1
        ret     m
.l3:    ld      de, chn_index_table
        ld      a, c
        or      e
        ld      e, a
        ld      a, (de)
        ld      c, a
        rla
        rla
        rla
        rla
        ld      de, dave_chn
        or      e
        ld      e, a
        ld      a, (de)
        rla
        jr      c, .l3
        ld      (hl), c
        ret

dave_play:
        ld      hl, dave_regs
        ld      de, dave_regs + 1
        ld      bc, 15
        ld      (hl), b
        ldir
        ld      hl, (midi_port_read)
        call    .l4
        push    ix
        ld      ix, dave_chn
.l1:    ld      a, (ix)                 ; chn->env_state
        or      a
        jp      m, .l3
        ld      b, a
        ld      l, (ix + 1)
        ld      h, (ix + 2)             ; HL = chn->env_ptr
        ld      a, (hl)                 ; vol_l
        ld      e, a
        inc     l
        and     0c0h
        ld      d, (hl)                 ; vol_r
        jr      z, .l7                  ; no envelope control flags?
        inc     d
        jr      z, .l2                  ; end of envelope?
        dec     d
        bit     6, b
        jr      nz, .l7                 ; releasing?
        add     a, a
        jr      nc, .l6                 ; begin loop?
        jr      z, .l5                  ; end of loop?
        inc     hl
        ld      (ix), 10h               ; hold single frame: enable release
        jr      .l8
.l2:    ld      (ix), 0a0h              ; end of envelope: chn->env_state = 160
        ld      (ix + 5), 0             ; chn->env_timer = 0
        ld      (ix + 6), 0
.l3:    ld      a, ixl
        add     a, 16
        ld      ixl, a
        cp      low (dave_chn + (DAVE_VIRT_CHNS * 16))
        jr      nz, .l1
        pop     ix
        call    set_dave_registers
        jp      update_chn_01_index
.l4:    jp      (hl)
.l5:    ld      l, (ix + 3)             ; end of loop
        ld      h, (ix + 4)             ; HL = chn->env_loop_ptr
        ld      e, (hl)                 ; vol_l
        inc     l
        ld      d, (hl)                 ; vol_r
        jr      .l7
.l6:    dec     l                       ; begin loop
        ld      (ix + 3), l             ; chn->env_loop_ptr = HL
        ld      (ix + 4), h
        inc     l
.l7:    inc     hl
        inc     l
        inc     hl
        ld      (ix + 1), l             ; chn->env_ptr = chn->env_ptr + 4
        ld      (ix + 2), h
        dec     hl
        dec     l
.l8:    inc     (ix + 5)                ; chn->env_timer
        jr      nz, .l9
        inc     (ix + 6)
.l9:    ld      a, ixl
        and     16 * (DAVE_VIRT_CHNS - 1)
        rra
        rra
        rra
        rra
        cp      2
        jr      z, .l11                 ; channel 2?
        cp      3
        jr      z, .l11                 ; channel 3?
        ld      c, a
        ld      a, (dave_chn0_index)
        cp      c
        jr      z, .l10                 ; channel 0?
        ld      a, (dave_chn1_index)
        cp      c
        jr      nz, .l3
.l10:   and     1
.l11:   or      low (dave_regs + 12)
        ld      c, a
        ld      b, high dave_regs
        push    bc
        ld      a, e
        and     3fh
        ld      e, a
        push    de
        ld      c, (hl)                 ; pitch bend
        inc     l
        ld      b, (hl)                 ; distortion
        call    dave_chn_calc_freq
        pop     bc                      ; C = vol_l, B = vol_r
        push    hl
        ld      h, (ix + 13)            ; chn->vol_l
        ld      a, h
        inc     a
        jr      nz, .l12
        push    bc
        ld      l, (ix + 9)             ; chn->veloc
        ld      h, (ix + 12)            ; chn->vol
        inc     l
        inc     h
        call    volume_mult
        ld      b, l                    ; B = vol * veloc
        ld      a, (ix + 11)            ; chn->pan
        add     a, a
        ld      l, a
        ld      h, high sin_table
        ld      c, (hl)                 ; C = vol_r
        cpl
        ld      l, a
        ld      l, (hl)                 ; L = vol_l
        ld      h, b
        call    volume_mult
        ld      h, b
        ld      b, l                    ; B = vol * veloc * vol_l
        ld      l, c
        call    volume_mult
        ld      h, b                    ; L = vol * veloc * vol_r
        ld      (ix + 13), h            ; chn->vol_l
        ld      (ix + 14), l            ; chn->vol_r
        pop     bc
.l12:   ld      l, c                    ; C = vol_l, B = vol_r, H = chn->vol_l
        call    volume_mult
        ld      c, l
        ld      l, b
        ld      h, (ix + 14)            ; chn->vol_r
        call    volume_mult
        ld      b, l
        pop     de                      ; DE = DAVE frequency
        pop     hl                      ; HL = &(dave_regs.volume_r[c])
        ld      (hl), b
        res     2, l
        ld      (hl), c
        ld      a, l
        and     3
        sub     8
        add     a, l
        ld      l, a                    ; HL = &(dave_regs.chn_freq[c])
        ld      (hl), e
        inc     l
        ld      (hl), d
        jp      .l3

