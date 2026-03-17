; neo1_cffa1_m2_blockdrv.s
;
; M4 CFFA1 write-verify test for Neo1-23.
;
; Provides:
;   CFBlockDriver  ($1800) - ProDOS block driver with STATUS, READ, WRITE
;   TestMain       ($1810) - exerciser:
;                            writes $AA pattern to block 1, reads/prints 16 bytes
;                            writes $55 pattern to block 2, reads/prints 16 bytes
;                            checks negative path on block 3
;
; CFBlockDriver call protocol (from CFFA1_API.s):
;   $42  pdCommandCode     0=STATUS, 1=READ, 2=WRITE
;   $43  pdUnitNumber      always 0
;   $44  pdIOBufferLow     buffer lo byte
;   $45  pdIOBufferHigh    buffer hi byte
;   $46  pdBlockNumberLow  block# lo byte
;   $47  pdBlockNumberHigh block# hi byte
;   JSR  CFBlockDriver
;   CLC + A=0  = ok
;   SEC + A=err = error
;
; Neo1 CFFA1 hardware registers (implemented by neo1_cffa1.c):
;   $AFDC  ID1 (read: $CF)
;   $AFDD  ID2 (read: $FA)
;   $AFF8  DATA register (streaming read/write)
;   $AFF9  ERROR register (read after command)
;   $AFFB  LBA[7:0]
;   $AFFC  LBA[15:8]
;   $AFFD  LBA[23:16]
;   $AFFE  LBA[31:24]
;   $AFFF  STATUS/COMMAND register (write=command, read=ATA status byte)
;             $00 = PRODOS_STATUS command
;             $01 = PRODOS_READ command
;             $02 = PRODOS_WRITE command
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
CMD_WRITE    = $02

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

; Test buffers - keep these above the assembled image to avoid self-overwrite.
; M4 grew the image into the old $1C00 region, so move both 512-byte buffers up.
; Write buffer uses $2000-$20FF (first 256) + $2100-$21FF (second 256)
; Read buffer uses $2200-$22FF (first 256) + $2300-$23FF (second 256)
WRITE_TEST_BUFFER = $2000     ; Where we stage test patterns before WRITE
READ_VERIFY_BUFFER= $2200     ; Where we read blocks back for verification

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
; 4. Fill buffer with $AA, call CFBlockDriver with WRITE to block 1
; 5. Call CFBlockDriver with READ from block 1 into different buffer
; 6. Print first 16 bytes of write/read buffers
; 7. Fill buffer with $55, WRITE block 2, READ block 2, print first 16 bytes
; 8. Negative: WRITE block 3 must return BADBLOCK ($2D)
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

        ; --- WRITE block 1 with $AA pattern ---
        ; Fill write buffer with $AA
        LDX #$00
        LDA #$00
        STA ZP_PTR_LO
        LDA #>WRITE_TEST_BUFFER
        STA ZP_PTR_HI
        LDY #$00
        LDA #$AA
FillBuffer:
        STA (ZP_PTR_LO),Y
        INY
        BNE FillBuffer
        INC ZP_PTR_HI        ; Now at $1D for second page
        LDY #$00
FillBuffer2:
        STA (ZP_PTR_LO),Y
        INY
        BNE FillBuffer2

        ; Issue WRITE command to block 1
        LDA #CMD_WRITE
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
        LDA #$01            ; block 1
        STA pdBlockNumberLow
        LDA #$00
        STA pdBlockNumberHigh
        LDA #<WRITE_TEST_BUFFER
        STA pdIOBufferLow
        LDA #>WRITE_TEST_BUFFER
        STA pdIOBufferHigh
        JSR CFBlockDriver
        BCC WriteOk
        ; Error
        PHA
        LDX #$00
WrErrLoop:
        LDA TxtWriteErr,X
        BEQ WrErrDone
        JSR Putc
        INX
        BNE WrErrLoop
WrErrDone:
        PLA
        JSR PrintHex
        JSR PrintCR
        BRK
WriteOk:
        LDX #$00
WrOkLoop:
        LDA TxtWriteOk,X
        BEQ WrOkDone
        JSR Putc
        INX
        BNE WrOkLoop
WrOkDone:

        ; --- READ block 1 back for verification ---
        LDA #CMD_READ
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
        LDA #$01            ; block 1
        STA pdBlockNumberLow
        LDA #$00
        STA pdBlockNumberHigh
        LDA #<READ_VERIFY_BUFFER
        STA pdIOBufferLow
        LDA #>READ_VERIFY_BUFFER
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

        ; --- Print first 16 bytes of WRITE buffer (what we wrote) ---
        LDX #$00
HexDumpWrite:
        LDA WRITE_TEST_BUFFER,X
        JSR PrintHex
        LDA #$20            ; space separator
        JSR Putc
        INX
        CPX #$10
        BNE HexDumpWrite
        JSR PrintCR

        ; --- Print first 16 bytes of READ buffer (what we read back) ---
        LDX #$00
HexDumpRead:
        LDA READ_VERIFY_BUFFER,X
        JSR PrintHex
        LDA #$20            ; space separator
        JSR Putc
        INX
        CPX #$10
        BNE HexDumpRead
        JSR PrintCR

        ; --- WRITE block 2 with $55 pattern ---
        LDX #$00
        LDA #$00
        STA ZP_PTR_LO
        LDA #>WRITE_TEST_BUFFER
        STA ZP_PTR_HI
        LDY #$00
        LDA #$55
FillBufferB2:
        STA (ZP_PTR_LO),Y
        INY
        BNE FillBufferB2
        INC ZP_PTR_HI
        LDY #$00
FillBufferB2_2:
        STA (ZP_PTR_LO),Y
        INY
        BNE FillBufferB2_2

        ; Issue WRITE command to block 2
        LDA #CMD_WRITE
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
        LDA #$02            ; block 2
        STA pdBlockNumberLow
        LDA #$00
        STA pdBlockNumberHigh
        LDA #<WRITE_TEST_BUFFER
        STA pdIOBufferLow
        LDA #>WRITE_TEST_BUFFER
        STA pdIOBufferHigh
        JSR CFBlockDriver
        BCC Write2Ok
        ; Error
        PHA
        LDX #$00
Wr2ErrLoop:
        LDA TxtWrite2Err,X
        BEQ Wr2ErrDone
        JSR Putc
        INX
        BNE Wr2ErrLoop
Wr2ErrDone:
        PLA
        JSR PrintHex
        JSR PrintCR
        BRK
Write2Ok:
        LDX #$00
Wr2OkLoop:
        LDA TxtWrite2Ok,X
        BEQ Wr2OkDone
        JSR Putc
        INX
        BNE Wr2OkLoop
Wr2OkDone:

        ; --- READ block 2 back for verification ---
        LDA #CMD_READ
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
        LDA #$02            ; block 2
        STA pdBlockNumberLow
        LDA #$00
        STA pdBlockNumberHigh
        LDA #<READ_VERIFY_BUFFER
        STA pdIOBufferLow
        LDA #>READ_VERIFY_BUFFER
        STA pdIOBufferHigh
        JSR CFBlockDriver
        BCC Read2Ok
        ; Error
        PHA
        LDX #$00
Rd2ErrLoop:
        LDA TxtRead2Err,X
        BEQ Rd2ErrDone
        JSR Putc
        INX
        BNE Rd2ErrLoop
Rd2ErrDone:
        PLA
        JSR PrintHex
        JSR PrintCR
        BRK
Read2Ok:
        LDX #$00
Rd2OkLoop:
        LDA TxtRead2Ok,X
        BEQ Rd2OkDone
        JSR Putc
        INX
        BNE Rd2OkLoop
Rd2OkDone:

        ; --- Print first 16 bytes of WRITE buffer (blk2 / $55) ---
        LDX #$00
HexDumpWrite2:
        LDA WRITE_TEST_BUFFER,X
        JSR PrintHex
        LDA #$20            ; space separator
        JSR Putc
        INX
        CPX #$10
        BNE HexDumpWrite2
        JSR PrintCR

        ; --- Print first 16 bytes of READ buffer (blk2 / expected $55) ---
        LDX #$00
HexDumpRead2:
        LDA READ_VERIFY_BUFFER,X
        JSR PrintHex
        LDA #$20            ; space separator
        JSR Putc
        INX
        CPX #$10
        BNE HexDumpRead2
        JSR PrintCR

        ; --- Negative test: WRITE block 3 should fail with BADBLOCK ($2D) ---
        LDA #CMD_WRITE
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
        LDA #$03            ; block 3 is intentionally unsupported in M4 backend
        STA pdBlockNumberLow
        LDA #$00
        STA pdBlockNumberHigh
        LDA #<WRITE_TEST_BUFFER
        STA pdIOBufferLow
        LDA #>WRITE_TEST_BUFFER
        STA pdIOBufferHigh
        JSR CFBlockDriver
        BCS NegHadErr

        ; Unexpected success
        LDX #$00
NegNoErrLoop:
        LDA TxtNegNoErr,X
        BEQ NegNoErrDone
        JSR Putc
        INX
        BNE NegNoErrLoop
NegNoErrDone:
        BRK

NegHadErr:
        CMP #ERR_BADBLOCK
        BNE NegWrongErr

        LDX #$00
NegOkLoop:
        LDA TxtNegOk,X
        BEQ NegOkDone
        JSR Putc
        INX
        BNE NegOkLoop
NegOkDone:
        LDA #ERR_BADBLOCK
        JSR PrintHex
        JSR PrintCR
        BRK

NegWrongErr:
        PHA
        LDX #$00
NegBadLoop:
        LDA TxtNegBadErr,X
        BEQ NegBadDone
        JSR Putc
        INX
        BNE NegBadLoop
NegBadDone:
        PLA
        JSR PrintHex
        JSR PrintCR
        BRK

;------------------------------------------------------------------------------
; DriverImpl  - ProDOS block driver over Neo1 CFFA1 shim
;
; Accepts pdCommandCode = $00 (STATUS), $01 (READ), or $02 (WRITE)
; Returns: CLC, A=0 on success
;          SEC, A=error code on failure
;------------------------------------------------------------------------------
DriverImpl:
        LDA pdCommandCode
        CMP #CMD_STATUS
        BEQ DoStatus
        CMP #CMD_READ
        BEQ DoRead
        CMP #CMD_WRITE
        BEQ DoWrite
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

DoWrite:
        ; Write 32-bit LBA from ZP parameter block
        LDA pdBlockNumberLow
        STA CFFA1_LBA0
        LDA pdBlockNumberHigh
        STA CFFA1_LBA1
        LDA #$00
        STA CFFA1_LBA2
        STA CFFA1_LBA3

        ; Issue WRITE command
        LDA #CMD_WRITE
        STA CFFA1_CMD

        ; Check for error before sending data
        LDA CFFA1_ERROR
        BNE WriteError

        ; Wait for DRQ (ready to accept data)
WaitDRQWrite:
        LDA CFFA1_STATUS
        AND #STATUS_DRQ
        BEQ WaitDRQWrite

        ; Stream 512 bytes from (pdIOBufferLow),Y to DATA register
        ; Two passes of 256 bytes for full 16-bit pointer
        LDA pdIOBufferLow
        STA ZP_PTR_LO
        LDA pdIOBufferHigh
        STA ZP_PTR_HI

        LDY #$00
WriteLo:
        LDA (ZP_PTR_LO),Y
        STA CFFA1_DATA
        INY
        BNE WriteLo

        INC ZP_PTR_HI
        LDY #$00
WriteHi:
        LDA (ZP_PTR_LO),Y
        STA CFFA1_DATA
        INY
        BNE WriteHi

        LDA #ERR_OK
        CLC
        RTS

WriteError:
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
        .asciiz "NEO1 CFFA1 M4 TEST"
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
TxtWriteOk:
        .byte $0D
        .asciiz "WRITE BLK1 OK"
TxtWriteErr:
        .byte $0D
        .asciiz "WRITE ERR:"
TxtReadOk:
        .byte $0D
        .asciiz "READ BLK1 OK WROTE/READ:"
TxtReadErr:
        .byte $0D
        .asciiz "READ ERR:"
TxtWrite2Ok:
        .byte $0D
        .asciiz "WRITE BLK2 OK"
TxtWrite2Err:
        .byte $0D
        .asciiz "WRITE2 ERR:"
TxtRead2Ok:
        .byte $0D
        .asciiz "READ BLK2 OK WROTE/READ:"
TxtRead2Err:
        .byte $0D
        .asciiz "READ2 ERR:"
TxtNegOk:
        .byte $0D
        .asciiz "NEG WRITE OK BADBLOCK:"
TxtNegNoErr:
        .byte $0D
        .asciiz "NEG WRITE FAIL:NOERR"
TxtNegBadErr:
        .byte $0D
        .asciiz "NEG WRITE FAIL:ERR="
