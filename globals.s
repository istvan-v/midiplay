
midiPlayDataBegin:

DAVE_VIRT_CHNS  equ     8
ENV_BUF_SIZE    equ     8192
FILE_BUF_SIZE   equ     31744

exdosFDCommand:
        defb    6
        defm    "EXDOS"
        defb    0fdh

    if DISPLAY_ENABLED != 0
programName:
        defm    "MIDIPLAY"
    endif

envelopeFileName:
        defb    12
        defm    "ENVELOPE.BIN"
defaultMIDIFileName:
        defb    12
        defm    "MIDIDATA.BIN"

errMsg_envNotFound:
        defm    "Error opening envelope file"
        defb    0
errMsg_envInvalid:
        defm    "Invalid envelope data size"
        defb    0
errMsg_midiNotFound:
        defm    "Error opening MIDI file"
        defb    0
errMsg_midiInvalid:
        defm    "Invalid MIDI data size"
        defb    0
errMsg_midiReadErr:
        defm    "Error reading MIDI file"
        defb    0

msg_creatingTables:
        defm    "Creating tables..."
msg_clear:
        defb    0

    if DISPLAY_ENABLED != 0
page1Segment:
        defb    00h
page2Segment:
        defb    00h
page3Segment:
        defb    00h
videoSegment:
        defb    00h
    endif

        align   256

fileNameBuffer:

decodeTablesBegin:

lengthDecodeTable       equ     decodeTablesBegin
offs1DecodeTable        equ     lengthDecodeTable + (nLengthSlots * 3)
offs2DecodeTable        equ     offs1DecodeTable + (nOffs1Slots * 3)
offs3DecodeTable        equ     offs2DecodeTable + (nOffs2Slots * 3)
decodeTablesEnd         equ     offs3DecodeTable + (maxOffs3Slots * 3)

; oct_table[n] =
;     (unsigned int) (250000.0 / (440.0 * pow(2.0, (n / 64.0 - 71.0) / 12.0))
;                     + 0.5)
oct_table:
        block   1536, 00h

; sin_table[n] = (unsigned char) (sin(n * PI * 0.5 / 255.0) * 181.02 + 0.5)
sin_table:
        block   256, 00h

        align   256

midi_pgm_layer2:
        block   256, 0ffh
midi_drum_layer2:
        block   256, 0ffh
pgm_env_offsets:
        block   256, 80h
drum_env_offsets:
        block   256, 80h
envelope_data:
        block   ENV_BUF_SIZE, 0ffh

        align   256

; midi_key_state[chn][key] = DAVE channel assigned + 1, or 0 if key not pressed
midi_key_state:
        block   2048, 00h
; midi_ctrl_state[chn][0] = controller 71, 76
; midi_ctrl_state[chn][1] = controller 70, 77
; midi_ctrl_state[chn][2] = controller 10
; midi_ctrl_state[chn][3] = controller 7
midi_ctrl_state:
        block   64, 00h
; 16 * (coarse * 256 + fine, 4580h = 440 Hz)
midi_chn_pitch:
        block   32, 00h
midi_chn_program:
        block   16, 00h
dave_midi_chn:
        block   DAVE_VIRT_CHNS, 0ffh

        align   128

; DAVE_VIRT_CHNS * struct DaveChannel:
;    0: env_state (b7 = off, b6 = releasing, b5 = not looped, b4 = can release)
; 1, 2: env_ptr (pointer to current envelope frame)
; 3, 4: env_loop_ptr (pointer to beginning of envelope loop)
; 5, 6: env_timer (frames since note on/off event)
; 7, 8: pitch (coarse * 256 + fine, 4580h = 440 Hz)
;    9: veloc (note on velocity)
;   10: dist (controller 71, 76 = distortion)
;   11: pan (controller 10)
;   12: vol (controller 7)
;   13: vol_l (current left volume calculated from velocity and controllers,
;              FFh if volume needs to be updated)
;   14: vol_r (current right volume)
;   15: aftertouch
dave_chn:
        block   DAVE_VIRT_CHNS * 16, 00h

        align   16

midi_dave_chn:
        block   16, 00h

dave_regs:
        block   16, 00h

        align   32

poly4_offs_table_15:
        defb      0,   0, 255,   0, 255,   1,   0,   0
        defb    255,   1,   0,   1,   0,   0,   1,   0

poly4_offs_table_5:
        defb      2,   1,   0, 255,   1,   0, 255,   1
        defb      0, 255,   1,   0, 255, 254,   3,   2

        align   32

midi_evt_handlers:
        defw    midi_clock, midi_unused                 ; F8h, F9h
        defw    midi_start, midi_continue               ; FAh, FBh
        defw    midi_stop, midi_unused                  ; FCh, FDh
        defw    midi_unused, midi_reset                 ; FEh, FFh
        defw    midi_note_off, midi_note_on             ; 80h, 90h
        defw    midi_poly_aft, midi_control_change      ; A0h, B0h
        defw    midi_program_change, midi_channel_aft   ; C0h, D0h
        defw    midi_pitch_bend, midi_unused            ; E0h, F0h

        align   8

chn_index_table:
        defb    4, 5, 4, 5, 6, 7, 0, 1

; 0, 4, 6
dave_chn0_index:
        defb    0
; 1, 5, 7
dave_chn1_index:
        defb    1

        align   2

; freq_mult_table[n] =
;     (unsigned int) (65536.0 * pow(0.5, 1.0 / (3.0 * (1 << n))) + 0.5)
freq_mult_table:
        defw    52016, 58386, 61858, 63670, 64596, 65065, 65300, 65418, 65477

        align   2
file_buf_ptr:
        defw    0

sprintf_d_table:
        defw    10000, 1000, 100, 10, 1

    if DISPLAY_ENABLED != 0
        align   16
displayRMTable:
        block   16, 69h
    endif

file_buf:

    if DISPLAY_ENABLED != 0
        include "pgmnames.s"

lptBorderVBlankBegin:
        defb    256 - 1, 82h, 63, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0
        defb    256 - ((287 - (28 * 9)) >> 1), 02h, 63, 0,  0, 0, 0, 0
        defb    0, 0, 0, 0, 0, 0, 0, 0
        defb    256 - 3, 00h, 63, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0
        defb    256 - 2, 00h, 6, 63,  0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0
        defb    256 - 1, 00h, 63, 32,  0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0
        defb    256 - 3, 00h, 63, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0
        defb    256 - 14, 02h, 6, 63,  0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0
        defb    256 - ((290 - (28 * 9)) >> 1), 03h, 63, 0,  0, 0, 0, 0
        defb    0, 0, 0, 0, 0, 0, 0, 0
lptBorderVBlankEnd:

lptBorderVBlankSize     equ     lptBorderVBlankEnd - lptBorderVBlankBegin
    endif

        block   (file_buf + FILE_BUF_SIZE + 4) - $, 00h

no_file_chooser:
        defb    0

        align   2
midi_port_read:
        defw    0
midi_file_buf:
        defw    0
midi_file_end:
        defw    0
midi_file_ptr:
        defw    0
midi_delta_time:
        defw    0
midi_prv_status:
        defb    0

midiPlayDataEnd:

