; Neo1 MSC Phase 2 loader (00-99 selection)
; Assembled for 65C02 and loaded at $0400

MSC_CMD      = $D014
MSC_SECT_LO  = $D015
MSC_SECT_HI  = $D016
MSC_DATA     = $D017
MSC_STATUS   = $D018
MSC_INDEX    = $D019
MSC_INFO     = $D01A

KBD          = $D010
KBDCR        = $D011
DSP          = $D012

CMD_DIR_OPEN = $10
CMD_DIR_NEXT = $11
CMD_OPEN_IND = $12
CMD_READ     = $03

INFO_VALID   = $01

ZP_PTR_LO    = $02
ZP_PTR_HI    = $03

TMP_INDEX    = $0200
TMP_STATUS   = $0201
TMP_TENS     = $0202
TMP_ONES     = $0203

        .org $0400

START:
        LDA #$0D
        JSR PUTC

        LDX #$00
MSG_FILES:
        LDA TXT_FILES,X
        BEQ FILES_DONE
        JSR PUTC
        INX
        BNE MSG_FILES
FILES_DONE:
        LDA #$0D
        JSR PUTC

        LDA #CMD_DIR_OPEN
        STA MSC_CMD
        JSR WAIT_READY
        BCC DIR_OPEN_OK
        JMP ERR
DIR_OPEN_OK:

        LDA #$00
        STA TMP_INDEX

LIST_LOOP:
        LDA TMP_INDEX
        CMP #$64
        BCS LIST_DONE

        LDA #CMD_DIR_NEXT
        STA MSC_CMD
        JSR WAIT_READY
        BCC DIR_NEXT_OK
        JMP ERR
DIR_NEXT_OK:

        LDA MSC_INFO
        AND #INFO_VALID
        BEQ LIST_DONE

        LDA TMP_INDEX
        JSR PRINT_DEC2

        LDA #$3A
        JSR PUTC
        LDA #$20
        JSR PUTC

PRINT_NAME:
        LDA MSC_DATA
        BEQ NAME_DONE
        JSR PUTC
        JMP PRINT_NAME

NAME_DONE:
        LDA #$0D
        JSR PUTC

        INC TMP_INDEX
        JMP LIST_LOOP

LIST_DONE:
        LDX #$00
MSG_SEL:
        LDA TXT_SEL,X
        BEQ SEL_DONE
        JSR PUTC
        INX
        BNE MSG_SEL
SEL_DONE:
        LDA #$0D
        JSR PUTC

GET_D1:
        JSR GETKEY
        JSR PUTC
        SEC
        SBC #$30
        CMP #$0A
        BCC D1_OK
        LDA #$0D
        JSR PUTC
        JMP GET_D1
D1_OK:
        STA TMP_TENS

GET_D2:
        JSR GETKEY
        JSR PUTC
        SEC
        SBC #$30
        CMP #$0A
        BCC D2_OK
        LDA #$0D
        JSR PUTC
        JMP GET_D2
D2_OK:
        STA TMP_ONES

        LDA #$0D
        JSR PUTC

        ; index = (tens*10) + ones
        LDA TMP_TENS
        ASL A
        STA TMP_INDEX      ; 2*tens
        ASL A              ; 4*tens
        ASL A              ; 8*tens
        CLC
        ADC TMP_INDEX      ; 10*tens
        CLC
        ADC TMP_ONES
        STA TMP_INDEX

        LDA TMP_INDEX
        STA MSC_INDEX

        LDA #CMD_OPEN_IND
        STA MSC_CMD
        JSR WAIT_READY
        BCC OPEN_OK
        JMP ERR
OPEN_OK:

        LDA #$00
        STA MSC_SECT_LO
        STA MSC_SECT_HI

        LDA #CMD_READ
        STA MSC_CMD
        JSR WAIT_READY
        BCC READ_OK
        JMP ERR
READ_OK:

        LDA #$00
        STA ZP_PTR_LO
        LDA #$03
        STA ZP_PTR_HI

        LDY #$00
COPY_LOOP:
        LDA MSC_DATA
        STA (ZP_PTR_LO),Y
        INY
        BNE COPY_LOOP

        JMP $0300

ERR:
        BRK

WAIT_READY:
WR_POLL:
        LDA MSC_STATUS
        BEQ WR_POLL
        STA TMP_STATUS
        AND #$80
        BEQ WR_OK
        SEC
        RTS
WR_OK:
        CLC
        RTS

PUTC:
        STA DSP
        RTS

GETKEY:
GK_WAIT:
        LDA KBDCR
        BPL GK_WAIT
        LDA KBD
        AND #$7F
        RTS

; A = 0..99, prints two decimal digits
PRINT_DEC2:
        LDX #$00
PD2_LOOP:
        CMP #$0A
        BCC PD2_DONE
        SEC
        SBC #$0A
        INX
        JMP PD2_LOOP
PD2_DONE:
        PHA
        TXA
        CLC
        ADC #$30
        JSR PUTC
        PLA
        CLC
        ADC #$30
        JSR PUTC
        RTS

TXT_FILES:
        .byte $46,$49,$4C,$45,$53,$00
TXT_SEL:
        .byte $53,$45,$4C,$20,$30,$30,$2D,$39,$39,$3F,$00
