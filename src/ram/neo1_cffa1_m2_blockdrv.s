; neo1_cffa1_m2_blockdrv.s
;
; M7.2 CFFA1 mini-menu: catalog, load-by-index, block inspect.
;
; Provides:
;   CFBlockDriver  ($1800) - ProDOS block driver with STATUS, READ, WRITE
;   TestMain       ($1810) - exerciser:
;                            CFFA1-style command loop with:
;                              C = catalog block 2 parse/list
;                              L = load selected entry by index with ADDR default
;                              B = block inspector (HHLL)
;                              Q = quit to WozMon
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
WOZMON_ENTRY = $FF00

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

CAT_FOUND    = $0208
CAT_KEY_LO   = $0209
CAT_KEY_HI   = $020A
CAT_EOF0     = $020B
CAT_EOF1     = $020C
CAT_EOF2     = $020D
CAT_TYPE     = $020E
CAT_FILETYPE = $020F
CAT_AUXLO    = $0210
CAT_AUXHI    = $0211

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
; 5. Attempt first-entry seedling payload load to $0300
; 6. Prompt for block HHLL (CR exits)
; 7. Call CFBlockDriver with READ from requested block into READ_VERIFY_BUFFER
; 8. Print first 128 bytes (hex, 16 bytes per line)
; 9. Loop back to prompt
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

MenuLoop:
        LDX #$00
MenuPromptLoop:
        LDA TxtMenuPrompt,X
        BEQ MenuPromptDone
        JSR Putc
        INX
        BNE MenuPromptLoop
MenuPromptDone:

        JSR GetKey
        JSR ToUpper
        PHA
        JSR Putc
        JSR PrintCR
        PLA

        CMP #'C'
        BEQ MenuDoCatalog
        CMP #'L'
        BEQ MenuDoLoad
        CMP #'B'
        BEQ MenuDoBlock
        CMP #'Q'
        BEQ MenuDoQuit
        CMP #'?'
        BEQ MenuDoCatalog

        LDX #$00
MenuUnknownLoop:
        LDA TxtMenuUnknown,X
        BEQ MenuUnknownDone
        JSR Putc
        INX
        BNE MenuUnknownLoop
MenuUnknownDone:
        JMP MenuLoop

MenuDoCatalog:
        JSR ShowCatalog
        JMP MenuLoop

MenuDoLoad:
        JSR MenuLoadByIndex
        JMP MenuLoop

MenuDoBlock:
        JSR RunBlockInspector
        JMP MenuLoop

MenuDoQuit:
        JMP WOZMON_ENTRY

;------------------------------------------------------------------------------
; RunBlockInspector - existing M6-style HHLL block inspector
; Returns to caller when user presses CR at prompt.
;------------------------------------------------------------------------------
RunBlockInspector:
        LDX #$00
PromptLoop:
        LDA TxtPrompt,X
        BEQ PromptDone
        JSR Putc
        INX
        BNE PromptLoop
PromptDone:

        JSR ReadHexWordOrCR
        BCC HaveBlock
        JSR PrintCR
        RTS

HaveBlock:
        JSR PrintCR

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
        JMP RunBlockInspector
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

        LDX #$00
HexDumpRead:
        LDA READ_VERIFY_BUFFER,X
        JSR PrintHex
        LDA #$20
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
        JMP RunBlockInspector

;------------------------------------------------------------------------------
; ReadCatalogBlock2 - read ProDOS root directory block #2 into READ_VERIFY_BUFFER
;------------------------------------------------------------------------------
ReadCatalogBlock2:
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
        RTS

;------------------------------------------------------------------------------
; ShowCatalog - read block 2 and print active directory entries
;------------------------------------------------------------------------------
ShowCatalog:
        LDA #$00
        STA CAT_FOUND

        LDX #$00
CatHdrLoop:
        LDA TxtCatHdr,X
        BEQ CatHdrDone
        JSR Putc
        INX
        BNE CatHdrLoop
CatHdrDone:

        JSR ReadCatalogBlock2
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
        BNE CatEntryNonZero
        JMP CatAdvance
CatEntryNonZero:
        STA TMP_N0
        AND #$0F
        BNE CatEntryHasName
        JMP CatAdvance
CatEntryHasName:
        STA TMP_NLEN

        ; Capture first active entry metadata (used as fallback/default).
        LDA CAT_FOUND
        BNE CatFirstDone
        LDA TMP_N0
        LSR A
        LSR A
        LSR A
        LSR A
        STA CAT_TYPE
        LDY #$11
        LDA (ZP_PTR_LO),Y
        STA CAT_KEY_LO
        LDY #$12
        LDA (ZP_PTR_LO),Y
        STA CAT_KEY_HI
        LDY #$15
        LDA (ZP_PTR_LO),Y
        STA CAT_EOF0
        LDY #$16
        LDA (ZP_PTR_LO),Y
        STA CAT_EOF1
        LDY #$17
        LDA (ZP_PTR_LO),Y
        STA CAT_EOF2
        LDY #$10
        LDA (ZP_PTR_LO),Y
        STA CAT_FILETYPE
        LDY #$1F
        LDA (ZP_PTR_LO),Y
        STA CAT_AUXLO
        LDY #$20
        LDA (ZP_PTR_LO),Y
        STA CAT_AUXHI
        LDA #$01
        STA CAT_FOUND
CatFirstDone:

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
; FindCatalogEntryByIndex - fetch metadata for one entry index (00..0C)
; In:  A = index (0..12)
; Out: C=0 metadata loaded into CAT_*; C=1 on error/empty entry
;------------------------------------------------------------------------------
FindCatalogEntryByIndex:
        CMP #$0D
        BCC FceRangeOk
        SEC
        RTS
FceRangeOk:
        STA TMP_N0

        JSR ReadCatalogBlock2
        BCC FceReadOk
        SEC
        RTS
FceReadOk:
        LDA READ_VERIFY_BUFFER+$23
        STA TMP_E_LEN
        LDA READ_VERIFY_BUFFER+$24
        STA TMP_E_COUNT

        LDA TMP_N0
        CMP TMP_E_COUNT
        BCC FceIdxOk
        SEC
        RTS
FceIdxOk:
        LDA #<(READ_VERIFY_BUFFER+$2B)
        STA ZP_PTR_LO
        LDA #>(READ_VERIFY_BUFFER+$2B)
        STA ZP_PTR_HI

        LDX TMP_N0
FceAddLoop:
        CPX #$00
        BEQ FcePtrReady
        CLC
        LDA ZP_PTR_LO
        ADC TMP_E_LEN
        STA ZP_PTR_LO
        BCC FceNoCarry
        INC ZP_PTR_HI
FceNoCarry:
        DEX
        JMP FceAddLoop
FcePtrReady:

        LDY #$00
        LDA (ZP_PTR_LO),Y
        BEQ FceEmpty
        STA TMP_N1
        AND #$0F
        BEQ FceEmpty

        ; Storage type in high nibble
        LDA TMP_N1
        LSR A
        LSR A
        LSR A
        LSR A
        STA CAT_TYPE

        LDY #$10
        LDA (ZP_PTR_LO),Y
        STA CAT_FILETYPE
        LDY #$11
        LDA (ZP_PTR_LO),Y
        STA CAT_KEY_LO
        LDY #$12
        LDA (ZP_PTR_LO),Y
        STA CAT_KEY_HI
        LDY #$15
        LDA (ZP_PTR_LO),Y
        STA CAT_EOF0
        LDY #$16
        LDA (ZP_PTR_LO),Y
        STA CAT_EOF1
        LDY #$17
        LDA (ZP_PTR_LO),Y
        STA CAT_EOF2
        LDY #$1F
        LDA (ZP_PTR_LO),Y
        STA CAT_AUXLO
        LDY #$20
        LDA (ZP_PTR_LO),Y
        STA CAT_AUXHI
        CLC
        RTS

FceEmpty:
        SEC
        RTS

;------------------------------------------------------------------------------
; MenuLoadByIndex - M7.2 load command
;------------------------------------------------------------------------------
MenuLoadByIndex:
        LDX #$00
MliPromptLoop:
        LDA TxtLoadIdxPrompt,X
        BEQ MliPromptDone
        JSR Putc
        INX
        BNE MliPromptLoop
MliPromptDone:

        JSR GetHexNibble
        STA TMP_N0
        JSR GetHexNibble
        STA TMP_N1
        JSR PrintCR

        LDA TMP_N0
        ASL A
        ASL A
        ASL A
        ASL A
        ORA TMP_N1
        JSR FindCatalogEntryByIndex
        BCC MliHaveEntry

        LDX #$00
MliNoEntLoop:
        LDA TxtLoadNoEntry,X
        BEQ MliNoEntDone
        JSR Putc
        INX
        BNE MliNoEntLoop
MliNoEntDone:
        RTS

MliHaveEntry:
        ; BA1 special path intentionally deferred for now.
        LDA CAT_FILETYPE
        CMP #$F1
        BNE MliNotBa1
        LDX #$00
MliBa1Loop:
        LDA TxtLoadBa1NYI,X
        BEQ MliBa1Done
        JSR Putc
        INX
        BNE MliBa1Loop
MliBa1Done:
        RTS

MliNotBa1:
        ; Default addr from auxtype.
        LDA CAT_AUXLO
        STA TMP_N2
        LDA CAT_AUXHI
        STA TMP_N3

        LDX #$00
MliAddrLoop:
        LDA TxtAddrPromptA,X
        BEQ MliAddrA_Done
        JSR Putc
        INX
        BNE MliAddrLoop
MliAddrA_Done:
        LDA TMP_N3
        JSR PrintHex
        LDA TMP_N2
        JSR PrintHex
        LDX #$00
MliAddrB_Loop:
        LDA TxtAddrPromptB,X
        BEQ MliAddrB_Done
        JSR Putc
        INX
        BNE MliAddrB_Loop
MliAddrB_Done:

        ; Enter keeps default; otherwise 4 hex digits override.
        JSR GetHexNibbleOrCR
        BCC MliAddrN0
        JMP MliDoLoad
MliAddrN0:
        STA TMP_N0
        JSR GetHexNibble
        STA TMP_N1
        JSR GetHexNibble
        STA TMP_N2
        JSR GetHexNibble
        STA TMP_N3

        LDA TMP_N0
        ASL A
        ASL A
        ASL A
        ASL A
        ORA TMP_N1
        PHA
        LDA TMP_N2
        ASL A
        ASL A
        ASL A
        ASL A
        ORA TMP_N3
        STA TMP_N2            ; destination lo
        PLA
        STA TMP_N3            ; destination hi

MliDoLoad:
        JSR PrintCR
        JSR LoadCurrentEntryAtAddr
        RTS

;------------------------------------------------------------------------------
; LoadCurrentEntryAtAddr
; Uses CAT_* metadata and destination in TMP_N3:TMP_N2
;------------------------------------------------------------------------------
LoadCurrentEntryAtAddr:
        ; Seedling and sapling.
        LDA CAT_TYPE
        CMP #$01
        BEQ LcaTypeOk
        CMP #$02
        BEQ LcaSapling
        LDX #$00
LcaTypeLoop:
        LDA TxtTypeErr,X
        BEQ LcaTypeDone
        JSR Putc
        INX
        BNE LcaTypeLoop
LcaTypeDone:
        LDA CAT_TYPE
        JSR PrintHex
        JSR PrintCR
        RTS

LcaSapling:
        JMP LoadSaplingAtAddr

LcaTypeOk:
        ; EOF <= 512 check
        LDA CAT_EOF2
        BEQ LcaSizeHiOk
        JMP LcaTooBig
LcaSizeHiOk:
        LDA CAT_EOF1
        CMP #$02
        BCC LcaSizeOk
        BEQ LcaSizeEq2
        JMP LcaTooBig
LcaSizeEq2:
        LDA CAT_EOF0
        BEQ LcaSizeOk
        JMP LcaTooBig

LcaSizeOk:
        ; Read key block
        LDA #CMD_READ
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
        LDA CAT_KEY_LO
        STA pdBlockNumberLow
        LDA CAT_KEY_HI
        STA pdBlockNumberHigh
        LDA #<READ_VERIFY_BUFFER
        STA pdIOBufferLow
        LDA #>READ_VERIFY_BUFFER
        STA pdIOBufferHigh
        JSR CFBlockDriver
        BCC LcaReadOk

        PHA
        LDX #$00
LcaReadErrLoop:
        LDA TxtLoadReadErr,X
        BEQ LcaReadErrDone
        JSR Putc
        INX
        BNE LcaReadErrLoop
LcaReadErrDone:
        PLA
        JSR PrintHex
        JSR PrintCR
        RTS

LcaReadOk:
        LDA TMP_N2
        STA ZP_PTR_LO
        LDA TMP_N3
        STA ZP_PTR_HI

        ; Copy first 256 (or less)
        LDA CAT_EOF1
        BEQ LcaLowOnly

        LDY #$00
LcaCopyPage0:
        LDA READ_VERIFY_BUFFER,Y
        STA (ZP_PTR_LO),Y
        INY
        BNE LcaCopyPage0

        INC ZP_PTR_HI
        LDA CAT_EOF1
        CMP #$01
        BEQ LcaCopyRemainder

        ; second full page when EOF == $0200
        LDY #$00
LcaCopyPage1Full:
        LDA READ_VERIFY_BUFFER+$100,Y
        STA (ZP_PTR_LO),Y
        INY
        BNE LcaCopyPage1Full
        JMP LcaSuccess

LcaCopyRemainder:
        LDA CAT_EOF0
        BEQ LcaSuccess
        STA TMP_N1
        LDY #$00
LcaCopyPage1Part:
        LDA READ_VERIFY_BUFFER+$100,Y
        STA (ZP_PTR_LO),Y
        INY
        DEC TMP_N1
        BNE LcaCopyPage1Part
        JMP LcaSuccess

LcaLowOnly:
        LDA CAT_EOF0
        BEQ LcaSuccess
        STA TMP_N1
        LDY #$00
LcaCopyLowPart:
        LDA READ_VERIFY_BUFFER,Y
        STA (ZP_PTR_LO),Y
        INY
        DEC TMP_N1
        BNE LcaCopyLowPart

LcaSuccess:
        LDX #$00
LcaOkLoop:
        LDA TxtSuccess,X
        BEQ LcaOkDone
        JSR Putc
        INX
        BNE LcaOkLoop
LcaOkDone:
        JSR PrintCR
        RTS

LcaTooBig:
        LDX #$00
LcaBigLoop:
        LDA TxtLoadBig,X
        BEQ LcaBigDone
        JSR Putc
        INX
        BNE LcaBigLoop
LcaBigDone:
        LDA CAT_EOF2
        JSR PrintHex
        LDA CAT_EOF1
        JSR PrintHex
        LDA CAT_EOF0
        JSR PrintHex
        JSR PrintCR
        RTS

;------------------------------------------------------------------------------
; LoadSaplingAtAddr
; CAT_KEY = index block, EOF = payload byte length, destination in TMP_N3:TMP_N2
;------------------------------------------------------------------------------
LoadSaplingAtAddr:
        ; Keep this compact: support sapling payload up to 2 data blocks (EOF <= $0400).
        LDA CAT_EOF2
        BEQ LsaChkEof1
        JMP LsaTooBig
LsaChkEof1:
        LDA CAT_EOF1
        CMP #$04
        BCC LsaSizeOk
        BEQ LsaSizeEq0400
        JMP LsaTooBig
LsaSizeEq0400:
        LDA CAT_EOF0
        BEQ LsaSizeOk
        JMP LsaTooBig

LsaSizeOk:
        LDA CAT_EOF0
        STA TMP_E_LEN
        LDA CAT_EOF1
        STA TMP_E_COUNT

        ; Read index block from CAT_KEY.
        LDA #CMD_READ
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
        LDA CAT_KEY_LO
        STA pdBlockNumberLow
        LDA CAT_KEY_HI
        STA pdBlockNumberHigh
        LDA #<READ_VERIFY_BUFFER
        STA pdIOBufferLow
        LDA #>READ_VERIFY_BUFFER
        STA pdIOBufferHigh
        JSR CFBlockDriver
        BCC LsaIdxOk
        JMP LsaReadErr

LsaIdxOk:
        ; Block pointer #0 from index block.
        LDY #$00
        LDA READ_VERIFY_BUFFER,Y
        STA TMP_N0
        LDA READ_VERIFY_BUFFER+$100,Y
        STA TMP_N1

        ; Cache block pointer #1 before DATA reads reuse READ_VERIFY_BUFFER.
        LDY #$01
        LDA READ_VERIFY_BUFFER,Y
        STA CAT_AUXLO
        LDA READ_VERIFY_BUFFER+$100,Y
        STA CAT_AUXHI

        LDA TMP_N0
        ORA TMP_N1
        BEQ LsaIdx0Missing

        ; Read first data block.
        LDA #CMD_READ
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
        LDA TMP_N0
        STA pdBlockNumberLow
        LDA TMP_N1
        STA pdBlockNumberHigh
        LDA #<READ_VERIFY_BUFFER
        STA pdIOBufferLow
        LDA #>READ_VERIFY_BUFFER
        STA pdIOBufferHigh
        JSR CFBlockDriver
        BCC LsaData0Ok
        JMP LsaReadErr

LsaData0Ok:
        ; Restore destination pointer clobbered by CFBlockDriver internals.
        LDA TMP_N2
        STA ZP_PTR_LO
        LDA TMP_N3
        STA ZP_PTR_HI
        JSR CopyChunkFromBuffer
        LDA ZP_PTR_LO
        STA TMP_N2
        LDA ZP_PTR_HI
        STA TMP_N3

        ; Remaining bytes?
        LDA TMP_E_LEN
        ORA TMP_E_COUNT
        BNE LsaNeedIdx1
        JMP LcaSuccess

LsaNeedIdx1:

        ; Need block pointer #1 (cached from index block).
        LDA CAT_AUXLO
        ORA CAT_AUXHI
        BEQ LsaIdx1Missing

        LDA #CMD_READ
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
        LDA CAT_AUXLO
        STA pdBlockNumberLow
        LDA CAT_AUXHI
        STA pdBlockNumberHigh
        LDA #<READ_VERIFY_BUFFER
        STA pdIOBufferLow
        LDA #>READ_VERIFY_BUFFER
        STA pdIOBufferHigh
        JSR CFBlockDriver
        BCC LsaData1Ok
        JMP LsaReadErr

LsaData1Ok:
        ; Restore destination pointer clobbered by CFBlockDriver internals.
        LDA TMP_N2
        STA ZP_PTR_LO
        LDA TMP_N3
        STA ZP_PTR_HI
        JSR CopyChunkFromBuffer
        LDA ZP_PTR_LO
        STA TMP_N2
        LDA ZP_PTR_HI
        STA TMP_N3
        JMP LcaSuccess

LsaIdx0Missing:
        LDX #$00
LsaIdx0Loop:
        LDA TxtLoadSaplingIdx0Err,X
        BEQ LsaIdx0Done
        JSR Putc
        INX
        BNE LsaIdx0Loop
LsaIdx0Done:
        JSR PrintCR
        RTS

LsaIdx1Missing:
        LDX #$00
LsaIdx1Loop:
        LDA TxtLoadSaplingIdx1Err,X
        BEQ LsaIdx1Done
        JSR Putc
        INX
        BNE LsaIdx1Loop
LsaIdx1Done:
        JSR PrintCR
        RTS

LsaTooBig:
        LDX #$00
LsaBigLoop:
        LDA TxtLoadSaplingBig,X
        BEQ LsaBigDone
        JSR Putc
        INX
        BNE LsaBigLoop
LsaBigDone:
        LDA CAT_EOF2
        JSR PrintHex
        LDA CAT_EOF1
        JSR PrintHex
        LDA CAT_EOF0
        JSR PrintHex
        JSR PrintCR
        RTS

LsaReadErr:
        PHA
        LDX #$00
LsaReadErrLoop:
        LDA TxtLoadReadErr,X
        BEQ LsaReadErrDone
        JSR Putc
        INX
        BNE LsaReadErrLoop
LsaReadErrDone:
        PLA
        JSR PrintHex
        JSR PrintCR
        RTS

;------------------------------------------------------------------------------
; CopyChunkFromBuffer
; Copy min(512, TMP_E_COUNT:TMP_E_LEN) bytes from READ_VERIFY_BUFFER to dest ptr
; in ZP_PTR_LO/HI, then decrement TMP_E_COUNT:TMP_E_LEN accordingly.
;------------------------------------------------------------------------------
CopyChunkFromBuffer:
        LDA TMP_E_COUNT
        CMP #$02
        BCS CcbCopy512
        CMP #$01
        BEQ CcbCopy256Plus

        ; 0..255 bytes
        LDA TMP_E_LEN
        BEQ CcbDone
        STA TMP_N0
        LDY #$00
CcbCopyLow:
        LDA READ_VERIFY_BUFFER,Y
        STA (ZP_PTR_LO),Y
        INY
        DEC TMP_N0
        BNE CcbCopyLow
        CLC
        LDA ZP_PTR_LO
        ADC TMP_E_LEN
        STA ZP_PTR_LO
        BCC CcbZeroRem
        INC ZP_PTR_HI
CcbZeroRem:
        LDA #$00
        STA TMP_E_LEN
        STA TMP_E_COUNT
CcbDone:
        RTS

CcbCopy256Plus:
        LDY #$00
CcbPage0:
        LDA READ_VERIFY_BUFFER,Y
        STA (ZP_PTR_LO),Y
        INY
        BNE CcbPage0
        INC ZP_PTR_HI

        LDA TMP_E_LEN
        BEQ CcbZeroRem
        STA TMP_N0
        LDY #$00
CcbPage1Part:
        LDA READ_VERIFY_BUFFER+$100,Y
        STA (ZP_PTR_LO),Y
        INY
        DEC TMP_N0
        BNE CcbPage1Part
        CLC
        LDA ZP_PTR_LO
        ADC TMP_E_LEN
        STA ZP_PTR_LO
        BCC CcbZeroRem
        INC ZP_PTR_HI
        JMP CcbZeroRem

CcbCopy512:
        LDY #$00
CcbFull0:
        LDA READ_VERIFY_BUFFER,Y
        STA (ZP_PTR_LO),Y
        INY
        BNE CcbFull0
        INC ZP_PTR_HI
        LDY #$00
CcbFull1:
        LDA READ_VERIFY_BUFFER+$100,Y
        STA (ZP_PTR_LO),Y
        INY
        BNE CcbFull1
        INC ZP_PTR_HI
        SEC
        LDA TMP_E_COUNT
        SBC #$02
        STA TMP_E_COUNT
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
        .asciiz "NEO1 CFFA1 M7.2 MINI MENU"
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
TxtMenuPrompt:
        .byte $0D
        .asciiz "CFFA1> "
TxtMenuUnknown:
        .byte $0D
        .asciiz "?"
TxtLoadIdxPrompt:
        .byte $0D
        .asciiz "LOAD INDEX (00-0C): "
TxtLoadNoEntry:
        .byte $0D
        .asciiz "LOAD ERR:NO ENTRY"
TxtLoadBa1NYI:
        .byte $0D
        .asciiz "LOAD BA1 NYI"
TxtAddrPromptA:
        .byte $0D
        .asciiz "ADDR ($"
TxtAddrPromptB:
        .asciiz "): "
TxtTypeErr:
        .byte $0D
        .asciiz "LOAD SKIP:TYPE="
TxtLoadReadErr:
        .byte $0D
        .asciiz "LOAD READ ERR:"
TxtLoadSaplingBig:
        .byte $0D
        .asciiz "LOAD SKIP:SAPLING EOF>0400 EOF="
TxtLoadSaplingIdx0Err:
        .byte $0D
        .asciiz "LOAD ERR:SAPLING IDX0"
TxtLoadSaplingIdx1Err:
        .byte $0D
        .asciiz "LOAD ERR:SAPLING IDX1"
TxtLoadBig:
        .byte $0D
        .asciiz "LOAD SKIP:EOF>0200 EOF="
TxtSuccess:
        .byte $0D
        .asciiz "00 SUCCESS"
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
