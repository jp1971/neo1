; neo1_cffa1_m2_blockdrv.s
;
; M2 CFFA1 compatibility test for Neo1-23.
;
; Provides:
;   CFBlockDriver  ($1800) - minimal ProDOS block driver stub
;   TestMain       ($1810) - exerciser that calls STATUS then READ block 0
;                            and prints 16 bytes of block data as hex
;
; CFBlockDriver call protocol (from CFFA1_API.s):
;   $42  pdCommandCode     0=STATUS, 1=READ
;   $43  pdUnitNumber      always 0
;   $44  pdIOBufferLow     destination lo byte
;   $45  pdIOBufferHigh    destination hi byte
;   $46  pdBlockNumberLow  block# lo byte
;   $47  pdBlockNumberHigh block# hi byte
;   JSR  CFBlockDriver
;   CLC + A=0  = ok
;   SEC + A=err = error
;
; Neo1 CFFA1 hardware registers (implemented by neo1_cffa1.c):
;   $AFDC  ID1 (read: $CF)
;   $AFDD  ID2 (read: $FA)
;   $AFF8  DATA register (streaming read; each read advances pointer)
;   $AFF9  ERROR register (read after command)
;   $AFFB  LBA[7:0]
;   $AFFC  LBA[15:8]
;   $AFFD  LBA[23:16]
;   $AFFE  LBA[31:24]
;   $AFFF  STATUS/COMMAND register (write=command, read=ATA status byte)
;             $00 = PRODOS_STATUS command
;             $01 = PRODOS_READ command
;             Status bits: bit7=BSY, bit6=DRDY, bit4=DSC, bit3=DRQ, bit0=ERR
;
; Run from WozMon:
;   1810R

.setcpu "65C02"

.export CFBlockDriver, TestMain

; ---- CFFA1 hardware registers ----
CFFA1_ID1    = $AFDC
CFFA1_ID2    = $AFDD
CFFA1_DATA   = $AFF8
CFFA1_ERROR  = $AFF9
CFFA1_LBA0   = $AFFB
CFFA1_LBA1   = $AFFC
CFFA1_LBA2   = $AFFD
CFFA1_LBA3   = $AFFE
CFFA1_CMD    = $AFFF
CFFA1_STATUS = $AFFF

; ATA status bits
STATUS_BSY   = $80
STATUS_DRDY  = $40
STATUS_DSC   = $10
STATUS_DRQ   = $08
STATUS_ERR   = $01

; ProDOS commands
CMD_STATUS   = $00
CMD_READ     = $01

; ProDOS error codes
ERR_OK       = $00
ERR_BADCMD   = $01
ERR_IO       = $27
ERR_NODEV    = $28
ERR_BADBLOCK = $2D

; ProDOS ZP parameter block (from CFFA1_API.s)
pdCommandCode    = $42
pdUnitNumber     = $43
pdIOBufferLow    = $44
pdIOBufferHigh   = $45
pdBlockNumberLow = $46
pdBlockNumberHigh= $47

; Neo1 I/O
KBD          = $D010
KBDCR        = $D011
DSP          = $D012
DSPCR        = $D013

; Test buffer placed above code+strings to avoid self-overwrite during 512-byte read
TEST_BUFFER  = $1A00

; Scratch ZP
ZP_PTR_LO    = $F0
ZP_PTR_HI    = $F1

        .org $1800

;------------------------------------------------------------------------------
; CFBlockDriver entrypoint at $1800.
; Keep this stable for callers, and trampoline to the actual implementation.
; We pad out to $1810 so TestMain is always physically at $1810.
;------------------------------------------------------------------------------
CFBlockDriver:
        JMP DriverImpl
        .res $1810-*, $EA

;------------------------------------------------------------------------------
; TestMain  ($1810)
;
; 1. Print banner
; 2. Check CFFA1 signature bytes
; 3. Call CFBlockDriver with STATUS
; 4. Call CFBlockDriver with READ (block 0 → TEST_BUFFER)
; 5. Print 16 bytes from TEST_BUFFER as hex pairs
;------------------------------------------------------------------------------
TestMain:
        ; --- Print banner ---
        LDX #$00
BannerLoop:
        LDA TxtBanner,X
        BEQ BannerDone
        JSR Putc
        INX
        BNE BannerLoop
BannerDone:

        ; --- Check signature ---
        LDA CFFA1_ID1
        CMP #$CF
        BNE SigFail
        LDA CFFA1_ID2
        CMP #$FA
        BNE SigFail

        LDX #$00
SigOkLoop:
        LDA TxtSigOk,X
        BEQ SigOkDone
        JSR Putc
        INX
        BNE SigOkLoop
SigOkDone:
        JMP DoStatusTest

SigFail:
        LDX #$00
SigFailLoop:
        LDA TxtSigFail,X
        BEQ SigFailDone
        JSR Putc
        INX
        BNE SigFailLoop
SigFailDone:
        BRK

        ; --- STATUS test ---
DoStatusTest:
        LDA #CMD_STATUS
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
        JSR CFBlockDriver
        BCC StatusOk
        ; Error
        PHA
        LDX #$00
StErrLoop:
        LDA TxtStatusErr,X
        BEQ StErrDone
        JSR Putc
        INX
        BNE StErrLoop
StErrDone:
        PLA
        JSR PrintHex
        JSR PrintCR
        BRK
StatusOk:
        LDX #$00
StOkLoop:
        LDA TxtStatusOk,X
        BEQ StOkDone
        JSR Putc
        INX
        BNE StOkLoop
StOkDone:

        ; --- READ block 0 test ---
        LDA #CMD_READ
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
        STA pdBlockNumberLow
        STA pdBlockNumberHigh
        LDA #<TEST_BUFFER
        STA pdIOBufferLow
        LDA #>TEST_BUFFER
        STA pdIOBufferHigh
        JSR CFBlockDriver
        BCC ReadOk
        ; Error
        PHA
        LDX #$00
RdErrLoop:
        LDA TxtReadErr,X
        BEQ RdErrDone
        JSR Putc
        INX
        BNE RdErrLoop
RdErrDone:
        PLA
        JSR PrintHex
        JSR PrintCR
        BRK
ReadOk:
        LDX #$00
RdOkLoop:
        LDA TxtReadOk,X
        BEQ RdOkDone
        JSR Putc
        INX
        BNE RdOkLoop
RdOkDone:

        ; --- Print 16 bytes from TEST_BUFFER as hex ---
        LDX #$00
HexDump:
        LDA TEST_BUFFER,X
        JSR PrintHex
        LDA #$20            ; space separator
        JSR Putc
        INX
        CPX #$10
        BNE HexDump
        JSR PrintCR
        BRK

;------------------------------------------------------------------------------
; DriverImpl  - minimal ProDOS block driver over Neo1 CFFA1 shim
;
; Accepts pdCommandCode = 0 (STATUS) or 1 (READ)
; Returns: CLC, A=0 on success
;          SEC, A=error code on failure
;------------------------------------------------------------------------------
DriverImpl:
        LDA pdCommandCode
        CMP #CMD_STATUS
        BEQ DoStatus
        CMP #CMD_READ
        BEQ DoRead
        LDA #ERR_BADCMD
        SEC
        RTS

DoStatus:
        ; Issue STATUS command to hardware
        LDA #CMD_STATUS
        STA CFFA1_CMD
        ; Check error register
        LDA CFFA1_ERROR
        BNE StatusError
        LDA #ERR_OK
        CLC
        RTS
StatusError:
        SEC
        RTS                     ; A already holds the error code

DoRead:
        ; Write 32-bit LBA from ZP parameter block
        LDA pdBlockNumberLow
        STA CFFA1_LBA0
        LDA pdBlockNumberHigh
        STA CFFA1_LBA1
        LDA #$00
        STA CFFA1_LBA2
        STA CFFA1_LBA3

        ; Issue READ command
        LDA #CMD_READ
        STA CFFA1_CMD

        ; Check for error before reading data
        LDA CFFA1_ERROR
        BNE ReadError

        ; Wait for DRQ (data ready)
WaitDRQ:
        LDA CFFA1_STATUS
        AND #STATUS_DRQ
        BEQ WaitDRQ

        ; Stream 512 bytes from DATA register into (pdIOBufferLow),Y
        ; Two passes of 256 bytes to handle full 16-bit pointer
        LDA pdIOBufferLow
        STA ZP_PTR_LO
        LDA pdIOBufferHigh
        STA ZP_PTR_HI

        LDY #$00
ReadLo:
        LDA CFFA1_DATA
        STA (ZP_PTR_LO),Y
        INY
        BNE ReadLo

        INC ZP_PTR_HI
        LDY #$00
ReadHi:
        LDA CFFA1_DATA
        STA (ZP_PTR_LO),Y
        INY
        BNE ReadHi

        LDA #ERR_OK
        CLC
        RTS

ReadError:
        SEC
        RTS                     ; A already holds the error code

;------------------------------------------------------------------------------
; Putc  - output char in A to Neo1 DSP
;------------------------------------------------------------------------------
Putc:
        BIT DSPCR
        BPL Putc            ; wait for display ready (bit 7)
        ORA #$80            ; set bit 7 as Apple-1 convention
        STA DSP
        RTS

;------------------------------------------------------------------------------
; PrintCR - output carriage return
;------------------------------------------------------------------------------
PrintCR:
        LDA #$0D
        JMP Putc

;------------------------------------------------------------------------------
; PrintHex - print byte in A as two hex digits
;------------------------------------------------------------------------------
PrintHex:
        PHA
        LSR A
        LSR A
        LSR A
        LSR A
        JSR HexNib
        PLA
        AND #$0F
HexNib:
        CMP #$0A
        BCC IsDigit
        CLC
        ADC #$07            ; 'A'-'9'-1
IsDigit:
        CLC
        ADC #$30            ; '0'
        JMP Putc

;------------------------------------------------------------------------------
; Strings
;------------------------------------------------------------------------------
TxtBanner:
        .byte $0D
        .asciiz "NEO1 CFFA1 M2 TEST"
TxtSigOk:
        .byte $0D
        .asciiz "SIG OK"
TxtSigFail:
        .byte $0D
        .asciiz "SIG FAIL"
TxtStatusOk:
        .byte $0D
        .asciiz "STATUS OK"
TxtStatusErr:
        .byte $0D
        .asciiz "STATUS ERR:"
TxtReadOk:
        .byte $0D
        .asciiz "READ OK BLOCK0:"
TxtReadErr:
        .byte $0D
        .asciiz "READ ERR:"
