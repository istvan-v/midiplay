
DISPLAY_ENABLED         equ     1
PANNED_NOTE_NEW         equ     1

        org     00f0h
        defw    0500h, prgEnd - main, 0, 0, 0, 0, 0, 0

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
        ld      hl, (0bffdh)
        ld      a, (0bfffh)
        ld      (page1Segment), hl
        ld      (page3Segment), a
        out     (0b3h), a
        ld      a, l
        out     (0b1h), a
        ld      a, h
        out     (0b2h), a
        ei
        ld      bc, 011ah               ; ST_FLAG
        ld      d, 0
        exos    16
        ld      bc, 011bh               ; BORD_VID
        ld      d, 0
        exos    16
        halt
        halt
        call    allocateMemory
        call    displayInit
.l1:    call    restoreLPT
        call    restoreIRQHandler
        call    load_midi_file
        call    z, load_envelopes
        di
        ld      hl, msg_creatingTables
        call    status_message
        call    daveInit
        ld      hl, msg_clear
        call    status_message
        call    setPlayerLPT
        call    midi_reset
        call    setIRQHandler
.l2:    ei
        halt
        ld      a, 4
        out     (0b5h), a
        in      a, (0b5h)
        cpl
        and     8bh
        jr      z, .l2
        di
        bit     1, a                    ; F8
        jp      nz, resetRoutine
        or      a
        jp      m, .l1                  ; F1
        and     08h                     ; F6
        jr      nz, .l3
        call    midi_stop               ; F4
        jr      .l2
.l3:    call    midi_file_rewind
        jr      .l2

allocateMemory:
        ld      hl, fileNameBuffer
        xor     a
.l1:    ld      (hl), a
        inc     hl
        exos    24
        jp      nz, resetRoutine
        ld      a, c
        cp      0fch
        jr      c, .l1
        ld      (videoSegment), a
.l2:    dec     hl
        ld      a, (hl)
        or      a
        ret     z
        ld      c, a
        exos    25
        jr      .l2

load_midi_file:
        ld      a, (no_file_chooser)
        or      a
        jr      nz, .l3
        ld      de, exdosFDCommand
        exos    26
        jr      nz, .l2
        ld      hl, fileNameBuffer
        push    hl
        ld      hl, 2045h               ; "E "
        push    hl
        ld      hl, 4c49h               ; "IL"
        push    hl
        ld      hl, 4607h               ; 7, "F"
        push    hl
        ld      hl, 0
        add     hl, sp
        ld      e, l
        ld      d, h
        exos    26
        pop     hl
        pop     hl
        pop     hl
        pop     hl
        jr      nz, .l2
        ld      de, fileNameBuffer
.l1:    ld      hl, file_buf
        ld      bc, FILE_BUF_SIZE
        jp      midi_file_load
.l2:    ld      a, 1
        ld      (no_file_chooser), a
.l3:    ld      de, defaultMIDIFileName
        jr      .l1

load_envelopes:
        ld      a, 1
        ld      de, envelopeFileName
        exos    1
        jr      nz, .l1
        inc     a
        ld      de, midi_pgm_layer2
        ld      bc, 1024 + 8192
        exos    6
        xor     0e4h                    ; .EOF
        jr      nz, .l2
        ld      a, c
        cp      low (8192 - 5)
        ld      a, b
        sbc     a, high (8192 - 5)
        jr      nc, .l2
        ld      a, 1
        exos    3
        ret
.l1:    ld      hl, errMsg_envNotFound
        jp      error_exit
.l2:    ld      hl, errMsg_envInvalid
        jp      error_exit

daveInit:
        call    testCPUFrequency        ; returns with B = 0
        ld      a, 25
        cp      l                       ; Carry = 1 if Z80 frequency > 5 MHz
        ld      a, 03h
        rla
        rla                             ; Z80 <= 5 MHz: 0Ch, > 5 MHz: 0Eh
        out     (0bfh), a
        jp      dave_init

; L = 1 kHz interrupts per video frame

testCPUFrequency:
        di
        ld      a, 04h
        out     (0bfh), a
        xor     a
        ld      b, a
        out     (0a7h), a
        ld      c, b
        call    .l1
        ld      l, b
.l1:    in      a, (0b4h)
        and     11h
        or      c
        rlca
        and     66h
        ld      c, a                    ; -ON--ON-
        rlca                            ; ON--ON--
        xor     c                       ; OXN-OXN-
        bit     2, a
        jr      z, .l2
        inc     l                       ; 1 kHz interrupt
.l2:    cp      0c0h
        jr      c, .l1                  ; not 50 Hz interrupt ?
        ret

setIRQHandler:
        di
        ld      a, 0c3h                 ; = JP nn
        ld      hl, irqRoutine
        ld      (0038h), a
        ld      (0039h), hl
        ld      a, 30h                  ; video interrupt only
        out     (0b4h), a
        ei
        ret

restoreIRQHandler:
        di
        ld      a, 0f5h                 ; = PUSH AF
        ld      hl, 1837h               ; = SCF, JR +d
        ld      (0038h), a
        ld      (0039h), hl
        ld      a, 3ch                  ; video + 1 Hz interrupt
        out     (0b4h), a
        ei
        ret

irqRoutine:
        push    af
        ld      a, 30h
        out     (0b4h), a
        push    bc
        push    de
        push    hl
        in      a, (0b3h)
        push    af
        ld      a, (page3Segment)
        out     (0b3h), a
        push    ix
        call    dave_play
        call    displayUpdate
        pop     ix
        pop     af
        out     (0b3h), a
        pop     hl
        pop     de
        pop     bc
        pop     af
        ei
        ret

vsync_wait:
.l1:    in      a, (0b4h)
        and     10h
        jr      z, .l1
.l2:    in      a, (0b4h)
        and     10h
        jr      nz, .l2
        ret

; HL = message address

status_message:
        in      a, (0b3h)
        push    af
        ld      a, 0ffh
        out     (0b3h), a
        ld      de, (0fff6h)
        ld      a, d
        or      high 0c000h
        ld      d, a
        ld      bc, 40
        xor     a
.l1:    cp      (hl)
        jr      z, .l2
        ldi
        jp      pe, .l1
        jr      .l3
.l2:    ld      a, 20h
        ld      (de), a
        inc     de
        dec     bc
        ld      a, c
        or      b
        jr      nz, .l2
.l3:    pop     af
        out     (0b3h), a
        ret

; HL = error message address

error_exit:
        di
        call    status_message
        ld      b, 100
.l1:    call    vsync_wait
        djnz    .l1

resetRoutine:
        di
        ld      sp, 3800h
        ld      a, 0ffh
        out     (0b2h), a
        ld      hl, resetRoutine
        ld      (0bff8h), hl
        call    restoreIRQHandler
        ld      c, 40h
        exos    0
        ld      a, 01h
        out     (0b3h), a
        ld      a, 6
        jp      0c00dh

        include "daveplay.s"
        include "midi_in.s"
        include "display.s"
        include "decompress_m2_new.s"
        include "globals.s"

prgEnd:

