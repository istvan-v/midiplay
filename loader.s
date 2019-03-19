
        org   00f0h
        defw  0500h, codeEnd - main, 0, 0, 0, 0, 0, 0

    macro exos n
        rst   30h
        defb  n
    endm

PRG_ADDR        equ     1000h
STACK_TOP       equ     PRG_ADDR - 0020h

main:
        di
        ld      de, 0000h               ; * (0102h) = lastAddr
        ld      sp, STACK_TOP
        ld      a, 0ffh
        out     (0b2h), a
        ld      hl, resetRoutine
        ld      (0bff8h), hl
        ld      hl, 0bffdh
        ld      a, d
        ld      de, page1Segment
.l1:    sub     high 4000h
        jr      c, .l2
        ldi
        jr      .l1
.l2:    ei
        ld      bc, 011bh               ; BORD_VID
        ld      d, 00h
        exos    16
        ld      bc, 011ch               ; BIAS_VID
        ld      d, 0f8h
        exos    16
        halt
        halt
        call    allocateMemory
        ld      hl, page1Segment
        ld      c, 0b1h
        outi
        inc     c
        outi                            ; videoSegment0
        inc     c
        outi                            ; videoSegment1
        call    daveInit
        di
        ld      hl, rstCodeBegin
        ld      de, rst08Routine
        ld      bc, rstCodeEnd - rstCodeBegin
        ldir
        call    setIRQHandler
        call    crt0main + 3            ; skip LD SP, 0
        jp      resetRoutine

allocateMemory:
        ld      hl, segmentTable        ; page1Segment
        call    .l1
        inc     hl                      ; page2Segment
        call    .l1
        inc     hl                      ; page3Segment
.l1:    xor     a
        cp      (hl)
        ret     nz
        exos    24
        ld      (hl), c
        ret     z
        jp      resetRoutine

printCharacter:
        ret

; -----------------------------------------------------------------------------

vsyncWait:
.l1:    in      a, (0b4h)
        and     10h
        jr      z, .l1
.l2:    in      a, (0b4h)
        and     10h
        jr      nz, .l2
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
        push    ix
        push    iy
        in      a, (0b1h)
        ld      l, a
        in      a, (0b2h)
        ld      h, a
        push    hl
        ld      hl, (page1Segment)
        ld      a, l
        out     (0b1h), a
        ld      a, h
        out     (0b2h), a
.l1:    call    .l2                     ; * irqCallback
        pop     hl
        ld      a, l
        out     (0b1h), a
        ld      a, h
        out     (0b2h), a
        pop     iy
        pop     ix
        pop     hl
        pop     de
        pop     bc
        pop     af
        ei
.l2:    ret

irqCallback     equ     irqRoutine.l1 + 1

resetRoutine:
        di
        ld      sp, STACK_TOP
        ld      a, 0ffh
        out     (0b2h), a
        ld      hl, resetRoutine
        ld      (0bff8h), hl
        call    restoreIRQHandler
        ld      hl, page1Segment
        ld      c, 0b1h
        outi
        inc     c
        outi
        inc     c
        outi
.l1:    ld      hl, irqRoutine.l2       ; * exitCallback
        ld      de, irqRoutine.l2
        ld      (exitCallback), de
        call    .l2
        ld      a, 0ffh
        out     (0b2h), a
        ld      c, 40h
        exos    0
        ld      a, 01h
        out     (0b3h), a
        ld      a, 6
        jp      0c00dh
.l2:    jp      (hl)

exitCallback    equ     resetRoutine.l1 + 1

setIRQCallback:
        ld      a, l
        or      h
        jr      nz, .l1
        ld      hl, irqRoutine.l2
.l1:    ld      (irqCallback), hl
        ret

setExitCallback:
        ld      a, l
        or      h
        jr      nz, .l1
        ld      hl, irqRoutine.l2
.l1:    ld      (exitCallback), hl
        ret

; -----------------------------------------------------------------------------

rstCodeBegin:
        phase   0008h

rst08Routine:
        cp      1
        jp      z, printCharacter
        jp      resetRoutine

        assert  $ == 0010h
rst10Routine:
        ex      (sp), hl
        push    af
        ld      a, (hl)
        inc     hl
        ld      (rst10Handler.l1 + 1), a
        pop     af
        ex      (sp), hl
        jp      rst10Handler
        assert  $ <= 0030h

        dephase
rstCodeEnd:

; -----------------------------------------------------------------------------

rst10Handler:
.l1:    jr      .l2                     ; *
.l2:    jp      resetRoutine            ; RST 10H  0
        jp      vsyncWait               ; RST 10H  3
        jp      setIRQHandler           ; RST 10H  6
        jp      restoreIRQHandler       ; RST 10H  9
        jp      setIRQCallback          ; RST 10H 12
        jp      setExitCallback         ; RST 10H 15

; -----------------------------------------------------------------------------

daveInit:
        call    testCPUFrequency        ; returns with B = 0
        ld      a, 25
        cp      l                       ; Carry = 1 if Z80 frequency > 5 MHz
        ld      a, 03h
        rla
        rla                             ; Z80 <= 5 MHz: 0Ch, > 5 MHz: 0Eh
        out     (0bfh), a
        ei
        ret

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

; -----------------------------------------------------------------------------

segmentTable:
page1Segment:
        defb    00h
page2Segment:
        defb    00h
page3Segment:
        defb    00h

        assert  $ <= (STACK_TOP - 1024)

        block   (PRG_ADDR - 0020h) - $, 00h
crt0main:
        block   PRG_ADDR - $, 0c9h      ; = RET

codeEnd:

