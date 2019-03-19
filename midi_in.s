
midi_reset:
        ld      de, dave_midi_chn
        ld      bc, DAVE_VIRT_CHNS * 256
.l1:    ld      a, c
        call    dave_channel_off
        ld      a, 0ffh
        ld      (de), a
        inc     e
        inc     c
        djnz    .l1
        ld      hl, midi_key_state
        ld      de, midi_key_state + 1
        ld      bc, 16 * 128 - 1
        ld      (hl), 0
        ldir
        ld      hl, midi_ctrl_state
        ld      de, midi_chn_program
        xor     a
        ld      b, 16
.l2:    ld      (hl), a                 ; distortion
        inc     l
        ld      (hl), a                 ; channel allocation control
        inc     l
        ld      (hl), 64                ; pan
        inc     l
        ld      (hl), 127               ; volume
        inc     l
        ld      (de), a
        inc     e
        djnz    .l2
        ld      de, midi_chn_pitch
        ld      bc, 0
.l3:    ld      a, low 0080h
        ld      (de), a
        inc     e
        xor     a
        ld      (de), a
        inc     e
        ld      a, c
        call    dave_assign_channel
        inc     c
        bit     4, c
        jr      z, .l3
        xor     a
        out     (0f6h), a
        ret

; A = MIDI channel (0 to 15)
; B = key (0 to 127)

midi_note_off:
        ld      c, a
        call    .l3
        ld      a, c
        cp      9
        jr      nz, .l1
        ld      l, b
        ld      h, high midi_drum_layer2
        jr      .l2
.l1:    ld      hl, midi_chn_program
        or      l
        ld      l, a
        ld      l, (hl)
        ld      h, high midi_pgm_layer2
        ld      a, c
.l2:    sla     l
        ld      d, (hl)
        inc     d
        ret     z
        dec     d
        add     a, d
        and     0fh
        ld      d, a
        inc     l
        ld      a, b
        add     a, (hl)
        and     7fh
        ld      b, a
        ld      a, d
.l3:    ld      l, b
        sla     l
        rra
        rr      l
        assert  (midi_key_state & 00ffh) == 0
        add     a, high midi_key_state
        ld      h, a
        ld      a, (hl)
        dec     a
        ret     m
        ld      (hl), 0
        ld      de, dave_midi_chn
        or      e
        ld      e, a
        and     DAVE_VIRT_CHNS - 1
        dave_channel_ptr  8
        ld      a, (hl)                 ; chn->pitch MSB
        res     3, l
        xor     b
        ret     nz
        dec     a
        ld      (de), a
        jp      dave_channel_release.l1

; A = MIDI channel (0 to 15)
; B = key (0 to 127)
; C = velocity (1 to 127)

midi_note_on:
        inc     c
        dec     c
        jr      z, midi_note_off        ; velocity == 0: note off
        ld      d, a
        push    bc
        call    .l3                     ; returns B = midi_chn
        ld      a, b
        pop     bc
        cp      9
        jr      nz, .l1
        ld      l, b
        ld      h, high midi_drum_layer2
        jr      .l2
.l1:    ld      hl, midi_chn_program
        or      l
        ld      l, a
        ld      l, (hl)
        ld      h, high midi_pgm_layer2
        and     0fh
.l2:    sla     l
        ld      d, (hl)
        inc     d
        ret     z
        dec     d
        add     a, d
        and     0fh
        ld      d, a
        inc     l
        ld      a, b
        add     a, (hl)
        and     7fh
        ld      b, a
        ld      a, d
.l3:    ld      l, b
        sla     l
        rra
        rr      l
        assert  (midi_key_state & 00ffh) == 0
        add     a, high midi_key_state
        ld      h, a
        push    hl
        ld      e, (hl)
        dec     e
        jp      m, .l4
        ld      a, e
        ld      hl, dave_midi_chn
        or      l
        ld      l, a
        ld      (hl), 0ffh
        ld      a, e
        call    dave_channel_off
.l4:    ld      a, d
        ld      d, b                    ; D = key
        ld      hl, midi_chn_program
        or      l
        ld      l, a
    if DISPLAY_ENABLED == 0
        ld      b, (hl)                 ; B = pgm, C = veloc
        rla
    else
        cp      low (midi_chn_program + 9)
        jr      nz, .l5
        ld      (hl), d
.l5:    ld      b, (hl)                 ; B = pgm, C = veloc
        add     a, a
    endif
        xor     low (midi_chn_pitch ^ ((midi_chn_program & 7fh) << 1))
        ld      l, a
        assert  ((midi_chn_pitch ^ midi_chn_program) & 0ff00h) == 0
        ld      e, (hl)                 ; DE = pitch
        inc     l
        ld      (hl), d
        rla
        xor     low (midi_ctrl_state ^ ((midi_chn_pitch & 7fh) << 1))
        ld      l, a                    ; HL = ctrls
        assert  ((midi_ctrl_state ^ midi_chn_pitch) & 0ff00h) == 0
        rra
        rra
        and     0fh                     ; A = midi_chn
        push    af
        call    dave_channel_on
        pop     bc
        ld      hl, dave_midi_chn
        or      l
        ld      l, a
        ld      (hl), b
        pop     hl
        sub     low (dave_midi_chn - 1)
        ld      (hl), a
        ret

; A = MIDI channel (0 to 15)
; B = key (0 to 127)
; C = value (0 to 127)

midi_poly_aft:
        ld      l, b
        sla     l
        rra
        rr      l
        assert  (midi_key_state & 00ffh) == 0
        add     a, high midi_key_state
        ld      h, a
        ld      a, (hl)
        dec     a
        ret     m
        dave_channel_ptr  8
        ld      a, (hl)                 ; chn->pitch MSB
        xor     b
        ret     nz
        dec     l
        set     3, l
        ld      a, c
        sub     (hl)                    ; chn->aftertouch
        ld      (hl), c
        res     1, l
        ld      (hl), 0ffh              ; chn->vol_l
        res     2, l
        add     a, (hl)                 ; chn->veloc
        ld      (hl), a
        ret

; A = MIDI channel (0 to 15)

midi_chn_reset_ctrl:
        ld      hl, midi_ctrl_state >> 2
        or      l
        ld      l, a
        and     0fh
        add     hl, hl
        add     hl, hl
        ld      (hl), 00h
        inc     l
        ld      (hl), 0
        inc     l
        ld      (hl), 64
        inc     l
        ld      (hl), 127
        ld      hl, midi_chn_pitch >> 1
        or      l
        ld      l, a
        and     0fh
        add     hl, hl
        ld      (hl), 80h
        ld      hl, dave_midi_chn
        ld      bc, DAVE_VIRT_CHNS * 256
.l1:    cp      (hl)
        jr      nz, .l2
        push    hl
        ld      e, b
        ld      d, a
        ld      a, c
        ld      b, 0
        call    dave_chn_distortion
        ld      a, c
        ld      b, 64
        call    dave_chn_set_pan
        ld      a, c
        ld      b, 127
        call    dave_chn_set_volume
        pop     hl
        ld      b, e
        ld      a, d
.l2:    inc     l
        inc     c
        djnz    .l1
        jp      dave_assign_channel

; A = MIDI channel (0 to 15)

midi_chn_notes_off:
        ld      hl, dave_midi_chn
        ld      bc, DAVE_VIRT_CHNS * 256
.l1:    cp      (hl)
        jr      nz, .l2
        push    af
        push    hl
        ld      a, c
        call    dave_channel_release
        pop     hl
        pop     af
.l2:    inc     l
        inc     c
        djnz    .l1
        ret

; A = MIDI channel (0 to 15)
; B = controller (0 to 127)
; C = value (0 to 127)

midi_control_change:
        ld      d, a
        ld      a, b
        cp      7
        jr      z, .l1                  ; volume?
        cp      10
        jr      z, .l2                  ; pan?
        cp      70
        jr      z, .l3                  ; channel allocation control?
        cp      71
        jr      z, .l4                  ; distortion
        cp      76
        jr      z, .l4
        cp      77
        jr      z, .l3
        cp      121
        ld      a, d
        jr      z, midi_chn_reset_ctrl
        ld      a, b
        cp      123
        ret     nz
        ld      a, d
        jr      midi_chn_notes_off
.l1:    ld      a, d
        ld      b, 3
        ld      de, dave_chn_set_volume
        jr      .l5
.l2:    ld      a, d
        ld      b, 2
        ld      de, dave_chn_set_pan
        jr      .l5
.l3:    ld      hl, midi_ctrl_state >> 2
        ld      a, l
        or      d
        ld      l, a
        add     hl, hl
        add     hl, hl
        inc     l
        ld      (hl), c
        ld      a, d
        ld      b, c
        jp      dave_assign_channel
.l4:    ld      a, d
        ld      b, 0
        ld      de, dave_chn_distortion
.l5:    ld      hl, midi_ctrl_state
        add     a, a
        add     a, a
        or      l
        or      b
        ld      l, a
        rra
        rra

; A & 15 = MIDI channel
; C = controller value
; HL = controller value address
; DE = daveplay routine address

midi_update_ctrl:
        ld      (hl), c

midi_update_aft:
        and     0fh
        ld      hl, dave_midi_chn
        ld      b, DAVE_VIRT_CHNS
.l1:    cp      (hl)
        jr      nz, .l2
        push    af
        push    de
        ld      a, l
        and     DAVE_VIRT_CHNS - 1
        ld      b, c
        ex      de, hl
        call    .l3
        ex      de, hl
        pop     de
        ld      a, low (dave_midi_chn + DAVE_VIRT_CHNS)
        sub     l
        ld      b, a
        pop     af
.l2:    inc     l
        djnz    .l1
        ret
.l3:    jp      (hl)

; A = MIDI channel (0 to 15)
; B = program (0 to 127)

midi_program_change:
        ld      hl, midi_chn_program
        or      l
        ld      l, a
        ld      (hl), b
        ret

; A = MIDI channel (0 to 15)
; B = value (0 to 127)

midi_channel_aft:
        ld      de, dave_chn_aftertouch
        ld      c, b
        jr      midi_update_aft

; A = MIDI channel (0 to 15)
; B = value & 127
; C = value >> 7

midi_pitch_bend:
        sla     b
        sla     b
        rl      c
        ld      hl, midi_chn_pitch >> 1
        or      l
        ld      l, a
        add     hl, hl
        ld      de, dave_channel_pitch
        jr      midi_update_ctrl

midi_clock:
midi_start:
midi_continue:
midi_unused:
        ret

midi_stop:
        ld      hl, dave_midi_chn
        ld      bc, DAVE_VIRT_CHNS * 256
.l1:    push    hl
        ld      a, c
        call    dave_channel_release
        pop     hl
        ld      (hl), 0ffh
        inc     l
        inc     c
        djnz    .l1
        ld      hl, midi_key_state
        ld      de, midi_key_state + 1
        ld      bc, 16 * 128 - 1
        ld      (hl), 0
        ldir
        ret

midi_read_hw:
.l1:    in      a, (0f6h)
        or      a
        ret     m
        in      a, (0f7h)               ; A = st
        or      a
        jp      p, .l1
        cp      0f0h
        jr      nc, .l4
        ld      c, 0f7h
        in      b, (c)                  ; B = d1
        res     7, b
        cp      0c0h
        jr      c, .l2
        cp      0e0h
        jr      c, .l3
.l2:    in      c, (c)                  ; C = d2
        res     7, c
.l3:    call    midi_handle_event
        jr      .l1
.l4:    call    midi_handle_sysevt      ; system message
        jr      .l1

midi_handle_sysevt:
        and     07h
        rla
        rla
        rla
        rla

; A = status, or (status & 7) << 4 if system message
; B = data 1
; C = data 2

midi_handle_event:
        ld      d, a
        and     0f0h
        rra
        rra
        rra
        assert  (midi_evt_handlers & 001fh) == 0
        or      low midi_evt_handlers
        ld      (.l1 + 1), a
.l1:    ld      hl, (midi_evt_handlers) ; *
        ld      a, d
        and     0fh
        jp      (hl)

midi_file_reset:
        ld      hl, (midi_file_buf)
        ld      (midi_file_ptr), hl
        xor     a
        ld      l, a
        ld      h, a
        ld      (midi_delta_time), hl
        ld      (midi_prv_status), a
        jp      midi_reset

midi_file_rewind:
        call    midi_file_reset
        ld      hl, (midi_port_read)
        ld      de, midi_read_file
        or      a
        sbc     hl, de
        ret     nz

; read and set delta time

midi_file_dtime:
        ld      hl, (midi_file_ptr)
        ld      de, (midi_file_end)
        ld      a, l
        cp      e
        ld      a, h
        sbc     a, d
        jr      nc, .l3                 ; end of file?
.l1:    ld      a, (hl)
        inc     hl
        or      a
        jp      m, .l2
        ld      (midi_file_ptr), hl
        ld      l, a
        ld      h, 0
        ld      (midi_delta_time), hl
        ret
.l2:    and     7fh
        rra
        ld      b, a
        ld      a, (hl)
        inc     hl
        ld      (midi_file_ptr), hl
        rla
        rrca
        ld      c, a
        ld      (midi_delta_time), bc
        ret
.l3:    call    midi_file_reset
        ld      hl, (midi_file_ptr)
        jr      .l1

midi_read_file_:
.l1:    ld      hl, (midi_file_ptr)
        ld      a, (hl)
        inc     hl
        or      a                       ; A = st
        jp      m, .l2
        ld      b, a                    ; B = d1
        ld      a, (midi_prv_status)    ; repeat previous status byte
        jr      .l3
.l2:    cp      0f0h
        jr      nc, .l8                 ; system message?
        ld      (midi_prv_status), a
        ld      b, (hl)
        inc     hl
        res     7, b
.l3:    cp      0c0h
        jr      c, .l4
        cp      0e0h
        jr      c, .l5
.l4:    ld      c, (hl)                 ; C = d2
        inc     hl
        res     7, c
.l5:    ld      (midi_file_ptr), hl
        call    midi_handle_event
.l6:    call    midi_file_dtime
.l7:    ld      hl, (midi_delta_time)   ; midi_read_file
        ld      a, l
        or      h
        jp      z, .l1
        dec     hl
        ld      (midi_delta_time), hl
        ret
.l8:    ld      (midi_file_ptr), hl
        jr      .l6

midi_read_file  equ     midi_read_file_.l7

; DE = file name address
; HL = file buffer address
; BC = file buffer size

midi_file_load:
        push    bc
        push    hl
        ld      hl, midi_read_hw
        ld      (midi_port_read), hl
        pop     hl
        ld      a, 1
        push    de
        exos    1
        jr      nz, .l3
        inc     a
        ld      e, l
        ld      d, h
        ld      bc, 16
        exos    6
        jr      nz, .l1
        inc     hl
        ld      a, (hl)
        dec     hl
        xor     6dh                     ; 'm'
        or      (hl)
        jr      z, .l8
.l1:    ld      a, 1                    ; headerless MIDI data + envelope.bin
        exos    3
        call    load_envelopes
        pop     de
        ld      a, 1
        exos    1
        jr      nz, .l5
        inc     a
        ld      e, l
        ld      d, h
        pop     bc
        exos    6
        xor     0e4h                    ; .EOF
        jr      nz, .l6
.l2:    ld      (midi_file_buf), hl
        ld      (midi_file_ptr), hl
        ld      (midi_file_end), de
        ex      de, hl
        sbc     hl, de
        ex      de, hl
        ld      (midi_prv_status), a    ; A = 0
        ld      a, e
        cp      3
        ld      a, d
        sbc     a, 0
        jr      c, .l6
        ld      a, 1
        exos    3
        ld      hl, midi_read_file
        ld      (midi_port_read), hl
        call    midi_file_dtime
        xor     a
        inc     a
        ret
.l3:    pop     de                      ; MIDI file not found
        pop     bc
        xor     a
        ret
.l4:    ld      hl, errMsg_envInvalid
        jr      .l7
.l5:    ld      hl, errMsg_midiNotFound
        jr      .l7
.l6:    ld      hl, errMsg_midiInvalid
.l7:    jp      error_exit
.l8:    pop     de
        push    hl
        ld      bc, 9
        add     hl, bc
        ld      a, (hl)
        cp      02h
        jr      z, .l10                 ; compressed format?
        or      a
        jr      nz, .l6
        ld      bc, 10000h - 5
        add     hl, bc
        ld      c, (hl)
        inc     hl
        ld      b, (hl)                 ; BC = envelope data size
        inc     hl
        ld      a, c
        cp      low (1024 + 6)
        ld      a, b
        sbc     a, high (1024 + 6)
        jr      c, .l4
        dec     bc
        ld      a, b
        inc     bc
        cp      high (1024 + 8192)
        jr      nc, .l4
        ld      de, midi_pgm_layer2
        ld      a, 1
        exos    6
        jr      nz, .l9
        ld      c, (hl)
        inc     hl
        ld      b, (hl)                 ; BC = MIDI data size
        pop     hl                      ; HL = file buffer address
        pop     de                      ; DE = file buffer size
        ld      a, c
        cp      3
        ld      a, b
        sbc     a, 0
        jr      c, .l6
        ld      a, c
        cp      e
        ld      a, b
        sbc     a, d
        jr      nc, .l6
        ld      a, 1
        ld      e, l
        ld      d, h
        exos    6
        jp      z, .l2
.l9:    ld      hl, errMsg_midiReadErr
        jr      .l8
.l10:   ld      bc, 10000h - 7
        add     hl, bc
        ld      c, (hl)
        inc     hl
        ld      b, (hl)                 ; BC = compressed size
        inc     hl
        inc     hl
        inc     hl
        ld      a, (hl)
        inc     hl
        cp      low FILE_BUF_SIZE
        ld      a, (hl)
        sbc     a, high FILE_BUF_SIZE
        jr      nc, .l6
        ld      hl, file_buf + FILE_BUF_SIZE + 4 + 1
        sbc     hl, bc                  ; Carry = 1
        ld      e, l
        ld      d, h
        ld      a, 1
        exos    6
        jr      nz, .l9
        ld      de, midi_pgm_layer2
        di
        ld      a, 0ch
        out     (0bfh), a
        call    decompressData
        pop     de                      ; DE = file buffer address
        pop     bc                      ; BC = file buffer size
        call    decompressData
        ld      a, 04h
        out     (0bfh), a
        ei
        ld      hl, file_buf
        xor     a
        jp      .l2

