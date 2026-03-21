; neo1_cffa1_m2_blockdrv.s
;
; M7 CFFA1 catalog + arbitrary-block inspector for Neo1-23.
;
; Provides:
;   CFBlockDriver  ($1800) - ProDOS block driver with STATUS, READ, WRITE
;   TestMain       ($1810) - exerciser:
;                            reads/parses catalog block 2 then enters block inspector loop
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

; Read buffer for 512-byte block reads.
READ_VERIFY_BUFFER= $2200

; Scratch ZP
ZP_PTR_LO    = $F0
ZP_PTR_HI    = $F1
TMP_N0       = $F2
TMP_N1       = $F3
TMP_N2       = $F4
TMP_N3       = $F5
TMP_E_LEN    = $F6
TMP_E_COUNT  = $F7
TMP_NLEN     = $F8

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
; 4. Read/parse ProDOS catalog block 2 and print entries
; 5. Prompt for block HHLL (CR exits)
; 6. Call CFBlockDriver with READ from requested block into READ_VERIFY_BUFFER
; 7. Print first 128 bytes (hex, 16 bytes per line)
; 8. Loop back to prompt
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
        JSR ShowCatalog

MainLoop:
        ; Prompt: BLK? HHLL, CR exits
        LDX #$00
PromptLoop:
        LDA TxtPrompt,X
        BEQ PromptDone
        JSR Putc
        INX
        BNE PromptLoop
PromptDone:

        ; Read 4 hex digits, high byte first. CR on first key exits.
        JSR ReadHexWordOrCR
        BCC HaveBlock
        BRK

HaveBlock:
        JSR PrintCR

        ; --- READ requested block from image ---
        LDA #CMD_READ
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
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
        JMP MainLoop
ReadOk:
        LDX #$00
RdHdrLoop:
        LDA TxtReadOkHead,X
        BEQ RdHdrDone
        JSR Putc
        INX
        BNE RdHdrLoop
RdHdrDone:
        LDA pdBlockNumberHigh
        JSR PrintHex
        LDA pdBlockNumberLow
        JSR PrintHex
        LDX #$00
RdTailLoop:
        LDA TxtReadOkTail,X
        BEQ RdTailDone
        JSR Putc
        INX
        BNE RdTailLoop
RdTailDone:

        ; --- Print first 128 bytes of read buffer (16 bytes per line) ---
        LDX #$00
HexDumpRead:
        LDA READ_VERIFY_BUFFER,X
        JSR PrintHex
        LDA #$20            ; space separator
        JSR Putc
        INX
        TXA
        AND #$0F
        BNE NoRowBreak
        JSR PrintCR
NoRowBreak:
        CPX #$80
        BNE HexDumpRead
        JSR PrintCR
        JMP MainLoop

;------------------------------------------------------------------------------
; ShowCatalog - read block 2 and print active directory entries
;------------------------------------------------------------------------------
ShowCatalog:
        LDX #$00
CatHdrLoop:
        LDA TxtCatHdr,X
        BEQ CatHdrDone
        JSR Putc
        INX
        BNE CatHdrLoop
CatHdrDone:

        LDA #CMD_READ
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
        LDA #$02
        STA pdBlockNumberLow
        LDA #$00
        STA pdBlockNumberHigh
        LDA #<READ_VERIFY_BUFFER
        STA pdIOBufferLow
        LDA #>READ_VERIFY_BUFFER
        STA pdIOBufferHigh
        JSR CFBlockDriver
        BCC CatReadOk
        PHA
        LDX #$00
CatErrLoop:
        LDA TxtCatErr,X
        BEQ CatErrDone
        JSR Putc
        INX
        BNE CatErrLoop
CatErrDone:
        PLA
        JSR PrintHex
        JSR PrintCR
        RTS

CatReadOk:
        ; Entry length and entries-per-block from ProDOS directory header.
        LDA READ_VERIFY_BUFFER+$23
        STA TMP_E_LEN
        LDA READ_VERIFY_BUFFER+$24
        STA TMP_E_COUNT

        ; Directory entries begin at offset $2B in the first catalog block.
        LDA #<(READ_VERIFY_BUFFER+$2B)
        STA ZP_PTR_LO
        LDA #>(READ_VERIFY_BUFFER+$2B)
        STA ZP_PTR_HI

        LDX #$00
CatEntryLoop:
        CPX TMP_E_COUNT
        BCC CatEntryDo
        JMP CatDone

CatEntryDo:

        LDY #$00
        LDA (ZP_PTR_LO),Y
        BEQ CatAdvance
        AND #$0F
        BEQ CatAdvance
        STA TMP_NLEN

        ; Print entry index (hex 00..)
        TXA
        JSR PrintHex
        LDA #$3A
        JSR Putc
        LDA #$20
        JSR Putc

        ; Print filename
        LDY #$01
        LDA TMP_NLEN
        STA TMP_N0
CatNameLoop:
        LDA TMP_N0
        BEQ CatNameDone
        LDA (ZP_PTR_LO),Y
        JSR Putc
        INY
        DEC TMP_N0
        JMP CatNameLoop
CatNameDone:
        LDA #$20
        JSR Putc

        ; Print key block pointer (offsets +$11/$12, little-endian)
        LDY #$00
CatKeyLblLoop:
        LDA TxtKey,Y
        BEQ CatKeyLblDone
        JSR Putc
        INY
        BNE CatKeyLblLoop
CatKeyLblDone:
        LDY #$12
        LDA (ZP_PTR_LO),Y
        JSR PrintHex
        LDY #$11
        LDA (ZP_PTR_LO),Y
        JSR PrintHex
        LDA #$20
        JSR Putc

        ; Print EOF (offset +$15..+$17, little-endian)
        LDY #$00
CatEofLblLoop:
        LDA TxtEof,Y
        BEQ CatEofLblDone
        JSR Putc
        INY
        BNE CatEofLblLoop
CatEofLblDone:
        LDY #$17
        LDA (ZP_PTR_LO),Y
        JSR PrintHex
        LDY #$16
        LDA (ZP_PTR_LO),Y
        JSR PrintHex
        LDY #$15
        LDA (ZP_PTR_LO),Y
        JSR PrintHex
        JSR PrintCR

CatAdvance:
        CLC
        LDA ZP_PTR_LO
        ADC TMP_E_LEN
        STA ZP_PTR_LO
        BCC CatAdvanceNoCarry
        INC ZP_PTR_HI
CatAdvanceNoCarry:
        INX
        JMP CatEntryLoop

CatDone:
        RTS

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
; GetKey - wait for key and return ASCII (bit7 stripped) in A
;------------------------------------------------------------------------------
GetKey:
KeyWait:
        LDA KBDCR
        BPL KeyWait
        LDA KBD
        AND #$7F
        RTS

;------------------------------------------------------------------------------
; ToUpper - normalize lowercase ASCII letter to uppercase
;------------------------------------------------------------------------------
ToUpper:
        CMP #$61            ; 'a'
        BCC ToUpperDone
        CMP #$7B            ; 'z'+1
        BCS ToUpperDone
        AND #$DF
ToUpperDone:
        RTS

;------------------------------------------------------------------------------
; HexCharToNibble - convert ASCII hex char in A to nibble in A
; Returns: C=0 valid, C=1 invalid
;------------------------------------------------------------------------------
HexCharToNibble:
        CMP #$30            ; '0'
        BCC HexBad
        CMP #$3A            ; '9'+1
        BCC HexDigit
        CMP #$41            ; 'A'
        BCC HexBad
        CMP #$47            ; 'F'+1
        BCS HexBad
        SEC
        SBC #$37            ; 'A' -> 10
        CLC
        RTS
HexDigit:
        SEC
        SBC #$30
        CLC
        RTS
HexBad:
        SEC
        RTS

;------------------------------------------------------------------------------
; GetHexNibble - read one valid hex nibble, echo accepted key
; Returns nibble in A, C=0
;------------------------------------------------------------------------------
GetHexNibble:
HexNibLoop:
        JSR GetKey
        JSR ToUpper
        TAX
        JSR HexCharToNibble
        BCS HexNibLoop
        PHA
        TXA
        JSR Putc
        PLA
        CLC
        RTS

;------------------------------------------------------------------------------
; GetHexNibbleOrCR - like GetHexNibble, but CR exits at first position
; Returns: C=1 if CR, else nibble in A and C=0
;------------------------------------------------------------------------------
GetHexNibbleOrCR:
HexNibOrCrLoop:
        JSR GetKey
        CMP #$0D
        BEQ HexNibOrCrExit
        JSR ToUpper
        TAX
        JSR HexCharToNibble
        BCS HexNibOrCrLoop
        PHA
        TXA
        JSR Putc
        PLA
        CLC
        RTS
HexNibOrCrExit:
        SEC
        RTS

;------------------------------------------------------------------------------
; ReadHexWordOrCR - read block number as HHLL hex
; Returns: C=1 if CR pressed at first prompt char (exit)
;          C=0 and stores pdBlockNumberHigh/Low on success
;------------------------------------------------------------------------------
ReadHexWordOrCR:
        JSR GetHexNibbleOrCR
        BCC GotN0
        SEC
        RTS
GotN0:
        STA TMP_N0
        JSR GetHexNibble
        STA TMP_N1
        JSR GetHexNibble
        STA TMP_N2
        JSR GetHexNibble
        STA TMP_N3

        ; High byte from N0:N1
        LDA TMP_N0
        ASL A
        ASL A
        ASL A
        ASL A
        ORA TMP_N1
        STA pdBlockNumberHigh

        ; Low byte from N2:N3
        LDA TMP_N2
        ASL A
        ASL A
        ASL A
        ASL A
        ORA TMP_N3
        STA pdBlockNumberLow

        CLC
        RTS

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
        .asciiz "NEO1 CFFA1 M7 CATALOG+INSPECT"
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
TxtCatHdr:
        .byte $0D
        .asciiz "CATALOG BLK 0002"
TxtCatErr:
        .byte $0D
        .asciiz "CAT READ ERR:"
TxtPrompt:
        .byte $0D
        .asciiz "BLK HHLL (CR=EXIT)? "
TxtKey:
        .asciiz "KEY="
TxtEof:
        .asciiz "EOF="
TxtReadOkHead:
        .byte $0D
        .asciiz "READ BLK "
TxtReadOkTail:
        .asciiz " OK HEX[00-7F]:"
TxtReadErr:
        .byte $0D
        .asciiz "READ ERR:"
