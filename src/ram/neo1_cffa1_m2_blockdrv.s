; neo1_cffa1_m2_blockdrv.s
;
; M8.2 CFFA1 mini-menu: catalog, load-by-index, block inspect,
; and CFFA1-style Write File prompts (existing-file overwrite path).
;
; Provides:
;   CFBlockDriver  ($1800) - ProDOS block driver with STATUS, READ, WRITE
;   TestMain       ($1810) - exerciser:
;                            CFFA1-style command loop with:
;                              C = catalog block 2 parse/list
;                              L = load selected entry by index (00-99)
;                              B = block inspector (HHLL)
;                              W = write file (CFFA1-style prompts)
;                              D = delete selected entry by index (00-99)
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
ERR_WPROTECT = $2B
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

; 512-byte staging buffer for block reads/writes.
READ_VERIFY_BUFFER= $3000

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
LOAD_NAME_LEN= $0212
LOAD_NAME_BUF= $0213
WRITE_REQ_TYPE = $0223
WRITE_LEN_LO = $0224
WRITE_LEN_HI = $0225
WRITE_SRC_LO = $0226
WRITE_SRC_HI = $0227
WRITE_NEED_BLOCKS = $0228
WRITE_ALLOC0_LO = $0229
WRITE_ALLOC0_HI = $022A
WRITE_ALLOC1_LO = $022B
WRITE_ALLOC1_HI = $022C
WRITE_ALLOC2_LO = $022D
WRITE_ALLOC2_HI = $022E
WRITE_STORAGE_TYPE = $022F
WRITE_BITMAP_LO = $0230
WRITE_BITMAP_HI = $0231
SEL_ENTRY_INDEX = $0232

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
        CMP #'W'
        BEQ MenuDoWrite
        CMP #'D'
        BEQ MenuDoDelete
        CMP #'Q'
        BEQ MenuDoQuit
        CMP #'?'
        BEQ MenuDoHelp

MenuDoHelp:
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

MenuDoWrite:
        JSR MenuWriteFile
        JMP MenuLoop

MenuDoDelete:
        JSR MenuDeleteFile
        JMP MenuLoop

MenuDoQuit:
        JMP WOZMON_ENTRY

;------------------------------------------------------------------------------
; MenuWriteFile
;
; CFFA1-style prompts:
;   WRITE FROM: $
;    LENGTH: $
;   TYPE (BIN): $   (CR accepts BIN default)
;   NAME:
;
; Current scope:
; - Overwrite existing seedling/sapling files only.
; - File must already exist in catalog block 2.
; - Requested TYPE must match existing filetype.
;------------------------------------------------------------------------------
MenuWriteFile:
        LDX #$00
MwfFromLoop:
        LDA TxtWriteFrom,X
        BEQ MwfFromDone
        JSR Putc
        INX
        BNE MwfFromLoop
MwfFromDone:

        JSR ReadHexWordOrCR
        BCC MwfFromOk
        JSR PrintCR
        RTS
MwfFromOk:
        ; Source pointer in TMP_N3:TMP_N2
        LDA pdBlockNumberLow
        STA TMP_N2
        STA WRITE_SRC_LO
        LDA pdBlockNumberHigh
        STA TMP_N3
        STA WRITE_SRC_HI
        JSR PrintCR

        LDX #$00
MwfLenLoop:
        LDA TxtWriteLen,X
        BEQ MwfLenDone
        JSR Putc
        INX
        BNE MwfLenLoop
MwfLenDone:

        JSR ReadHexWordOrCR
        BCC MwfLenOk
        JSR PrintCR
        RTS
MwfLenOk:
        ; Remaining length in TMP_E_COUNT:TMP_E_LEN
        LDA pdBlockNumberLow
        STA TMP_E_LEN
        STA WRITE_LEN_LO
        LDA pdBlockNumberHigh
        STA TMP_E_COUNT
        STA WRITE_LEN_HI
        JSR PrintCR

        ; LENGTH 0000 -> treat as cancel.
        LDA TMP_E_LEN
        ORA TMP_E_COUNT
        BNE MwfHaveLen
        RTS
MwfHaveLen:

        ; TYPE prompt (default BIN=$06)
        LDA #$06
        STA WRITE_REQ_TYPE
        LDX #$00
MwfTypeLoop:
        LDA TxtWriteType,X
        BEQ MwfTypeDone
        JSR Putc
        INX
        BNE MwfTypeLoop
MwfTypeDone:
        JSR GetHexNibbleOrCR
        BCC MwfTypeN0
        JMP MwfTypeAccept
MwfTypeN0:
        STA TMP_N1
        JSR GetHexNibble
        STA TMP_N0
        LDA TMP_N1
        ASL A
        ASL A
        ASL A
        ASL A
        ORA TMP_N0
        STA WRITE_REQ_TYPE
MwfTypeAccept:
        JSR PrintCR

        ; NAME prompt
        LDX #$00
MwfNameLoop:
        LDA TxtWriteName,X
        BEQ MwfNameDone
        JSR Putc
        INX
        BNE MwfNameLoop
MwfNameDone:

        JSR ReadFilenameOrCR
        JSR PrintCR
        BCC MwfHaveName
        RTS
MwfHaveName:

        ; Find existing entry, or create a new one.
        JSR FindCatalogEntryByName
        BCC MwfHaveEntry

        ; Not found — create new directory entry and allocate blocks.
        JSR CreateEntryForWrite
        BCC MwfCreatedEntry
        RTS

MwfCreatedEntry:
        JMP MwfTypeMatch

MwfHaveEntry:
        ; TYPE must match existing entry type for this stage.
        ; Transitional compatibility: treat existing filetype $00 as BIN ($06).
        LDA CAT_FILETYPE
        BNE MwfTypeNormDone
        LDA #$06
        STA CAT_FILETYPE
MwfTypeNormDone:

        LDA WRITE_REQ_TYPE
        CMP CAT_FILETYPE
        BEQ MwfTypeMatch
        LDX #$00
MwfTypeErrLoop:
        LDA TxtWriteTypeErr,X
        BEQ MwfTypeErrDone
        JSR Putc
        INX
        BNE MwfTypeErrLoop
MwfTypeErrDone:
        LDA CAT_FILETYPE
        JSR PrintHex
        JSR PrintCR
        RTS

MwfTypeMatch:
        ; Restore write parameters that catalog lookup reuses as temporaries.
        LDA WRITE_SRC_LO
        STA TMP_N2
        LDA WRITE_SRC_HI
        STA TMP_N3
        LDA WRITE_LEN_LO
        STA TMP_E_LEN
        LDA WRITE_LEN_HI
        STA TMP_E_COUNT
        JSR WriteCurrentEntryFromAddr
        RTS

;------------------------------------------------------------------------------
; MenuDeleteFile
;
; CFFA1 D command: delete a file from the ProDOS root directory.
; Frees all data blocks in the bitmap, zeroes the directory entry,
; decrements the volume file count, and writes both blocks back.
;
; Supports seedling (type 1) and sapling (type 2) only.
;------------------------------------------------------------------------------
MenuDeleteFile:
        ; -- Prompt: DELETE:
        LDX #$00
MdfPromptLoop:
        LDA TxtDeletePrompt,X
        BEQ MdfPromptDone
        JSR Putc
        INX
        BNE MdfPromptLoop
MdfPromptDone:
        JSR ReadDec2OrCR
        BCS MdfCancel
        STA SEL_ENTRY_INDEX
        JSR PrintCR
        JMP MdfHaveIndex
MdfCancel:
        JSR PrintCR
        RTS
MdfHaveIndex:
        ; -- Find catalog entry by index (also reads block 2 into READ_VERIFY_BUFFER).
        LDA SEL_ENTRY_INDEX
        STA TMP_N0
        JSR FindCatalogEntryByIndex
        BCC MdfFound
        LDX #$00
MdfNoFileLoop:
        LDA TxtDeleteNoFile,X
        BEQ MdfNoFileDone
        JSR Putc
        INX
        BNE MdfNoFileLoop
MdfNoFileDone:
        RTS
MdfFound:

        ; -- Load bitmap block address from volume header ($27/$28).
        ; (ZP_PTR_LO/HI already points at entry but we re-derive it later
        ;  via a second FindCatalogEntryByName after the bitmap write.)
        LDA READ_VERIFY_BUFFER+$27
        STA WRITE_BITMAP_LO
        LDA READ_VERIFY_BUFFER+$28
        STA WRITE_BITMAP_HI

        ; -- Save directory block contents to a second scratch area.
        ; We only have one READ_VERIFY_BUFFER (512 bytes at $3000).
        ; Strategy: free sapling index/data blocks first (needs buffer),
        ; then reload dir block and zero the entry.

        ; Save key block from CAT_KEY_LO/HI now, before buffer is reused.
        LDA CAT_KEY_LO
        STA WRITE_ALLOC0_LO
        LDA CAT_KEY_HI
        STA WRITE_ALLOC0_HI
        LDA CAT_TYPE
        STA WRITE_STORAGE_TYPE

        ; -- For sapling: read index block, collect data-block addresses.
        LDA WRITE_STORAGE_TYPE
        CMP #$02
        BNE MdfFreeSeedling

        ; Read sapling index block into buffer.
        LDA #CMD_READ
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
        LDA WRITE_ALLOC0_LO
        STA pdBlockNumberLow
        LDA WRITE_ALLOC0_HI
        STA pdBlockNumberHigh
        LDA #<READ_VERIFY_BUFFER
        STA pdIOBufferLow
        LDA #>READ_VERIFY_BUFFER
        STA pdIOBufferHigh
        JSR CFBlockDriver
        BCC MdfIdxOk
        ; read error — still attempt dir cleanup
        JMP MdfFreeIndexBlock
MdfIdxOk:
        ; data block 0 (lo byte from $00, hi byte from $100)
        LDA READ_VERIFY_BUFFER+$00
        STA WRITE_ALLOC1_LO
        LDA READ_VERIFY_BUFFER+$100
        STA WRITE_ALLOC1_HI
        ; data block 1 (lo byte from $01, hi byte from $101)
        LDA READ_VERIFY_BUFFER+$01
        STA WRITE_ALLOC2_LO
        LDA READ_VERIFY_BUFFER+$101
        STA WRITE_ALLOC2_HI

        ; Free data block 0 (if non-zero).
        LDA WRITE_ALLOC1_LO
        ORA WRITE_ALLOC1_HI
        BEQ MdfSkipData0
        LDA WRITE_ALLOC1_LO
        STA TMP_N2
        LDA WRITE_ALLOC1_HI
        STA TMP_N3
        JSR MdfFreeOneBlock
MdfSkipData0:
        ; Free data block 1 (if non-zero).
        LDA WRITE_ALLOC2_LO
        ORA WRITE_ALLOC2_HI
        BEQ MdfSkipData1
        LDA WRITE_ALLOC2_LO
        STA TMP_N2
        LDA WRITE_ALLOC2_HI
        STA TMP_N3
        JSR MdfFreeOneBlock
MdfSkipData1:

MdfFreeIndexBlock:
        ; Free the index block itself.
        LDA WRITE_ALLOC0_LO
        STA TMP_N2
        LDA WRITE_ALLOC0_HI
        STA TMP_N3
        JSR MdfFreeOneBlock
        JMP MdfWriteBitmap

MdfFreeSeedling:
        ; Free the single data block.
        LDA WRITE_ALLOC0_LO
        STA TMP_N2
        LDA WRITE_ALLOC0_HI
        STA TMP_N3
        JSR MdfFreeOneBlock

MdfWriteBitmap:
        ; -- Write updated bitmap block back to disk.
        LDA #CMD_WRITE
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
        LDA WRITE_BITMAP_LO
        STA pdBlockNumberLow
        LDA WRITE_BITMAP_HI
        STA pdBlockNumberHigh
        LDA #<READ_VERIFY_BUFFER
        STA pdIOBufferLow
        LDA #>READ_VERIFY_BUFFER
        STA pdIOBufferHigh
        JSR CFBlockDriver
        ; ignore write error — proceed to zero dir entry

        ; -- Reload directory block into buffer.
        JSR ReadCatalogBlock2
        BCC MdfDirReloaded
        LDX #$00
MdfDirErrLoop:
        LDA TxtDeleteErr,X
        BEQ MdfDirErrDone
        JSR Putc
        INX
        BNE MdfDirErrLoop
MdfDirErrDone:
        RTS
MdfDirReloaded:

        ; -- Re-find the entry so ZP_PTR_LO/HI is valid in the fresh buffer.
        LDA SEL_ENTRY_INDEX
        STA TMP_N0
        JSR FindCatalogEntryByIndex
        BCC MdfFoundAgain
        ; Entry vanished — already clean, done.
        RTS
MdfFoundAgain:

        ; -- Zero all bytes of the entry (TMP_E_LEN bytes).
        LDY #$00
MdfZeroLoop:
        LDA #$00
        STA (ZP_PTR_LO),Y
        INY
        CPY TMP_E_LEN
        BCC MdfZeroLoop

        ; -- Decrement volume file count at header offset $25/$26.
        LDA READ_VERIFY_BUFFER+$25
        BNE MdfDecLo
        DEC READ_VERIFY_BUFFER+$26
MdfDecLo:
        DEC READ_VERIFY_BUFFER+$25

        ; -- Write directory block back.
        LDA #CMD_WRITE
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
        BCC MdfDone
        LDX #$00
MdfDirWrErrLoop:
        LDA TxtDeleteErr,X
        BEQ MdfDirWrErrDone
        JSR Putc
        INX
        BNE MdfDirWrErrLoop
MdfDirWrErrDone:
        RTS
MdfDone:
        LDX #$00
MdfOkLoop:
        LDA TxtDeleteOk,X
        BEQ MdfOkDone
        JSR Putc
        INX
        BNE MdfOkLoop
MdfOkDone:
        RTS

;------------------------------------------------------------------------------
; MdfFreeOneBlock
;
; In:  TMP_N2/TMP_N3 = block number to free
;      READ_VERIFY_BUFFER contains loaded bitmap block
; Out: corresponding bit set (freed) in READ_VERIFY_BUFFER
;      (caller must write bitmap back when done)
;------------------------------------------------------------------------------
MdfFreeOneBlock:
        ; Byte index = block_number / 8.  High half if block >= 2048.
        ; Bit mask: bit 7 is block 0 within byte (MSB first).
        LDA TMP_N3
        BEQ MfobLow            ; block < $0100 * 8 = 2048, stay in low half
        ; High half (blocks 2048-4095): byte index = (block - 2048) / 8
        ;   = (TMP_N2 / 8) + (TMP_N3-1)*32, offset into READ_VERIFY_BUFFER+$100
        ; Simplified: TMP_N3 always 0 or 1 for volumes <=4096 blocks.
        ; byte_in_half = TMP_N2 / 8 (upper 5 bits), bit_mask from low 3 bits
        LDA TMP_N2
        AND #$07
        TAX                    ; bit index within byte (0=MSB)
        LDA TMP_N2
        LSR A
        LSR A
        LSR A
        TAY                    ; byte index within the 256-byte half
        LDA #$80
MfobHiShift:
        CPX #$00
        BEQ MfobHiApply
        LSR A
        DEX
        BNE MfobHiShift
MfobHiApply:
        ORA READ_VERIFY_BUFFER+$100,Y
        STA READ_VERIFY_BUFFER+$100,Y
        RTS
MfobLow:
        LDA TMP_N2
        AND #$07
        TAX
        LDA TMP_N2
        LSR A
        LSR A
        LSR A
        TAY
        LDA #$80
MfobLoShift:
        CPX #$00
        BEQ MfobLoApply
        LSR A
        DEX
        BNE MfobLoShift
MfobLoApply:
        ORA READ_VERIFY_BUFFER,Y
        STA READ_VERIFY_BUFFER,Y
        RTS

;------------------------------------------------------------------------------
; CreateEntryForWrite
;
; Create a new root-directory file entry for current NAME/TYPE/LENGTH/SOURCE.
; Supports seedling (<=512 bytes) and sapling (<=1024 bytes) for now.
;
; Out: CLC on success (CAT_* populated for write path), SEC on error.
;------------------------------------------------------------------------------
CreateEntryForWrite:
        ; Determine storage type and blocks needed from requested length.
        LDA WRITE_LEN_HI
        CMP #$02
        BCC CefSeedling
        BEQ CefSeedEq2
        CMP #$04
        BCC CefSapling
        BEQ CefSapEq4
        JMP CefTooBig

CefSeedEq2:
        LDA WRITE_LEN_LO
        BEQ CefSeedling
        JMP CefSapling

CefSapEq4:
        LDA WRITE_LEN_LO
        BEQ CefSapling
        JMP CefTooBig

CefSeedling:
        LDA #$01
        STA WRITE_STORAGE_TYPE
        LDA #$01
        STA WRITE_NEED_BLOCKS
        JMP CefAlloc

CefSapling:
        LDA #$02
        STA WRITE_STORAGE_TYPE
        LDA #$03
        STA WRITE_NEED_BLOCKS

CefAlloc:
        JSR AllocateBlocksForCreate
        BCC CefHaveBlocks
        RTS

CefHaveBlocks:
        ; Read root directory block 2.
        JSR ReadCatalogBlock2
        BCC CefDirReadOk
        PHA
        LDX #$00
CefDirReadErrLoop:
        LDA TxtWriteDirErr,X
        BEQ CefDirReadErrDone
        JSR Putc
        INX
        BNE CefDirReadErrLoop
CefDirReadErrDone:
        PLA
        JSR PrintHex
        JSR PrintCR
        SEC
        RTS

CefDirReadOk:
        ; Locate first free entry in block 2.
        LDA READ_VERIFY_BUFFER+$23
        STA TMP_E_LEN
        LDA READ_VERIFY_BUFFER+$24
        STA TMP_E_COUNT
        LDA #<(READ_VERIFY_BUFFER+$2B)
        STA ZP_PTR_LO
        LDA #>(READ_VERIFY_BUFFER+$2B)
        STA ZP_PTR_HI

CefFindSlotLoop:
        LDA TMP_E_COUNT
        BEQ CefNoSlot
        LDY #$00
        LDA (ZP_PTR_LO),Y
        BEQ CefSlotFound
        AND #$0F
        BEQ CefSlotFound

        CLC
        LDA ZP_PTR_LO
        ADC TMP_E_LEN
        STA ZP_PTR_LO
        BCC CefSlotNoCarry
        INC ZP_PTR_HI
CefSlotNoCarry:
        DEC TMP_E_COUNT
        JMP CefFindSlotLoop

CefNoSlot:
        LDX #$00
CefNoSlotLoop:
        LDA TxtWriteDirFull,X
        BEQ CefNoSlotDone
        JSR Putc
        INX
        BNE CefNoSlotLoop
CefNoSlotDone:
        SEC
        RTS

CefSlotFound:
        ; Clear entry bytes.
        LDY #$00
CefClrLoop:
        LDA #$00
        STA (ZP_PTR_LO),Y
        INY
        CPY TMP_E_LEN
        BCC CefClrLoop

        ; Name length and storage nibble.
        LDA WRITE_STORAGE_TYPE
        ASL A
        ASL A
        ASL A
        ASL A
        ORA LOAD_NAME_LEN
        LDY #$00
        STA (ZP_PTR_LO),Y

        ; Copy filename bytes.
        LDY #$01
CefNameLoop:
        CPY LOAD_NAME_LEN
        BEQ CefNameLast
        BCS CefNameDone
        LDA LOAD_NAME_BUF-1,Y
        STA (ZP_PTR_LO),Y
        INY
        JMP CefNameLoop
CefNameLast:
        LDA LOAD_NAME_BUF-1,Y
        STA (ZP_PTR_LO),Y
CefNameDone:

        ; Filetype.
        LDY #$10
        LDA WRITE_REQ_TYPE
        STA (ZP_PTR_LO),Y

        ; Key block and data pointers.
        LDA WRITE_STORAGE_TYPE
        CMP #$01
        BEQ CefSetSeedling

        ; Sapling: key block = alloc0 (index block)
        LDY #$11
        LDA WRITE_ALLOC0_LO
        STA (ZP_PTR_LO),Y
        INY
        LDA WRITE_ALLOC0_HI
        STA (ZP_PTR_LO),Y
        JMP CefSetCounts

CefSetSeedling:
        ; Seedling: key block = alloc0 (data block)
        LDY #$11
        LDA WRITE_ALLOC0_LO
        STA (ZP_PTR_LO),Y
        INY
        LDA WRITE_ALLOC0_HI
        STA (ZP_PTR_LO),Y

CefSetCounts:
        ; Block count.
        LDY #$13
        LDA WRITE_NEED_BLOCKS
        STA (ZP_PTR_LO),Y
        INY
        LDA #$00
        STA (ZP_PTR_LO),Y

        ; EOF (24-bit)
        LDY #$15
        LDA WRITE_LEN_LO
        STA (ZP_PTR_LO),Y
        INY
        LDA WRITE_LEN_HI
        STA (ZP_PTR_LO),Y
        INY
        LDA #$00
        STA (ZP_PTR_LO),Y

        ; Access
        LDY #$1E
        LDA #$C3
        STA (ZP_PTR_LO),Y

        ; Auxtype = source address
        LDY #$1F
        LDA WRITE_SRC_LO
        STA (ZP_PTR_LO),Y
        INY
        LDA WRITE_SRC_HI
        STA (ZP_PTR_LO),Y

        ; Header pointer = root dir block 2
        LDY #$25
        LDA #$02
        STA (ZP_PTR_LO),Y
        INY
        LDA #$00
        STA (ZP_PTR_LO),Y

        ; Increment volume file count in header.
        INC READ_VERIFY_BUFFER+$25
        BNE CefNoCarryFC
        INC READ_VERIFY_BUFFER+$26
CefNoCarryFC:

        ; Write updated directory block.
        LDA #CMD_WRITE
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
        BCC CefDirWriteOk
        PHA
        LDX #$00
CefDirWrErrLoop:
        LDA TxtWriteDirErr,X
        BEQ CefDirWrErrDone
        JSR Putc
        INX
        BNE CefDirWrErrLoop
CefDirWrErrDone:
        PLA
        JSR PrintHex
        JSR PrintCR
        SEC
        RTS

CefDirWriteOk:
        ; For sapling, initialize index block with data pointers.
        LDA WRITE_STORAGE_TYPE
        CMP #$02
        BNE CefPopulateCat

        LDX #$00
CefIdxZero:
        STZ READ_VERIFY_BUFFER,X
        STZ READ_VERIFY_BUFFER+$100,X
        INX
        BNE CefIdxZero

        ; pointer 0 -> alloc1
        LDA WRITE_ALLOC1_LO
        STA READ_VERIFY_BUFFER+$00
        LDA WRITE_ALLOC1_HI
        STA READ_VERIFY_BUFFER+$100
        ; pointer 1 -> alloc2
        LDA WRITE_ALLOC2_LO
        STA READ_VERIFY_BUFFER+$01
        LDA WRITE_ALLOC2_HI
        STA READ_VERIFY_BUFFER+$101

        LDA #CMD_WRITE
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
        LDA WRITE_ALLOC0_LO
        STA pdBlockNumberLow
        LDA WRITE_ALLOC0_HI
        STA pdBlockNumberHigh
        LDA #<READ_VERIFY_BUFFER
        STA pdIOBufferLow
        LDA #>READ_VERIFY_BUFFER
        STA pdIOBufferHigh
        JSR CFBlockDriver
        BCC CefPopulateCat
        PHA
        LDX #$00
CefIdxErrLoop:
        LDA TxtWriteAllocErr,X
        BEQ CefIdxErrDone
        JSR Putc
        INX
        BNE CefIdxErrLoop
CefIdxErrDone:
        PLA
        JSR PrintHex
        JSR PrintCR
        SEC
        RTS

CefPopulateCat:
        LDA WRITE_STORAGE_TYPE
        STA CAT_TYPE
        LDA WRITE_REQ_TYPE
        STA CAT_FILETYPE
        LDA WRITE_LEN_LO
        STA CAT_EOF0
        LDA WRITE_LEN_HI
        STA CAT_EOF1
        LDA #$00
        STA CAT_EOF2
        LDA WRITE_SRC_LO
        STA CAT_AUXLO
        LDA WRITE_SRC_HI
        STA CAT_AUXHI

        ; key block for write path
        LDA WRITE_ALLOC0_LO
        STA CAT_KEY_LO
        LDA WRITE_ALLOC0_HI
        STA CAT_KEY_HI

        CLC
        RTS

CefTooBig:
        LDX #$00
CefTooBigLoop:
        LDA TxtWriteTooBig,X
        BEQ CefTooBigDone
        JSR Putc
        INX
        BNE CefTooBigLoop
CefTooBigDone:
        SEC
        RTS

;------------------------------------------------------------------------------
; AllocateBlocksForCreate
;
; In:  WRITE_NEED_BLOCKS = 1(seedling) or 3(sapling)
; Out: WRITE_ALLOC0..2 set, bitmap updated on disk.
;------------------------------------------------------------------------------
AllocateBlocksForCreate:
        ; Read block 2 to find bitmap start block.
        JSR ReadCatalogBlock2
        BCC AbfcDirOk
        PHA
        LDX #$00
AbfcDirErrLoop:
        LDA TxtWriteDirErr,X
        BEQ AbfcDirErrDone
        JSR Putc
        INX
        BNE AbfcDirErrLoop
AbfcDirErrDone:
        PLA
        JSR PrintHex
        JSR PrintCR
        SEC
        RTS

AbfcDirOk:
        ; Bitmap block pointer from volume header offsets $27/$28.
        LDA READ_VERIFY_BUFFER+$27
        STA WRITE_BITMAP_LO
        STA TMP_N0
        LDA READ_VERIFY_BUFFER+$28
        STA WRITE_BITMAP_HI
        STA TMP_N1

        ; Read first bitmap block.
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
        BCC AbfcBmpOk
        PHA
        LDX #$00
AbfcBmpErrLoop:
        LDA TxtWriteAllocErr,X
        BEQ AbfcBmpErrDone
        JSR Putc
        INX
        BNE AbfcBmpErrLoop
AbfcBmpErrDone:
        PLA
        JSR PrintHex
        JSR PrintCR
        SEC
        RTS

AbfcBmpOk:
        ; clear allocation outputs
        STZ WRITE_ALLOC0_LO
        STZ WRITE_ALLOC0_HI
        STZ WRITE_ALLOC1_LO
        STZ WRITE_ALLOC1_HI
        STZ WRITE_ALLOC2_LO
        STZ WRITE_ALLOC2_HI

        ; candidate block number in TMP_N2/TMP_N3, allocated count in TMP_N1
        STZ TMP_N2
        STZ TMP_N3
        STZ TMP_N1

        ; scan first 256 bytes (blocks 0..2047)
        LDY #$00
AbfcScanLoByte:
        LDA READ_VERIFY_BUFFER,Y
        STA TMP_N0
        LDA #$80
        STA TMP_NLEN
AbfcScanLoBit:
        LDA TMP_N0
        AND TMP_NLEN
        BEQ AbfcLoNoAlloc

        ; allocate current candidate block
        LDA TMP_NLEN
        EOR #$FF
        AND TMP_N0
        STA TMP_N0
        LDA TMP_N0
        STA READ_VERIFY_BUFFER,Y

        LDA TMP_N1
        BEQ AbfcStore0
        CMP #$01
        BEQ AbfcStore1
        ; else store2
        LDA TMP_N2
        STA WRITE_ALLOC2_LO
        LDA TMP_N3
        STA WRITE_ALLOC2_HI
        JMP AbfcStored
AbfcStore0:
        LDA TMP_N2
        STA WRITE_ALLOC0_LO
        LDA TMP_N3
        STA WRITE_ALLOC0_HI
        JMP AbfcStored
AbfcStore1:
        LDA TMP_N2
        STA WRITE_ALLOC1_LO
        LDA TMP_N3
        STA WRITE_ALLOC1_HI
AbfcStored:
        INC TMP_N1
        LDA TMP_N1
        CMP WRITE_NEED_BLOCKS
        BEQ AbfcWriteBitmap

AbfcLoNoAlloc:
        ; candidate++
        INC TMP_N2
        BNE AbfcLoCandOk
        INC TMP_N3
AbfcLoCandOk:

        LSR TMP_NLEN
        BNE AbfcScanLoBit
        INY
        BNE AbfcScanLoByte

        ; scan second 256 bytes (blocks 2048..4095)
        LDY #$00
AbfcScanHiByte:
        LDA READ_VERIFY_BUFFER+$100,Y
        STA TMP_N0
        LDA #$80
        STA TMP_NLEN
AbfcScanHiBit:
        LDA TMP_N0
        AND TMP_NLEN
        BEQ AbfcHiNoAlloc

        ; allocate current candidate block
        LDA TMP_NLEN
        EOR #$FF
        AND TMP_N0
        STA TMP_N0
        LDA TMP_N0
        STA READ_VERIFY_BUFFER+$100,Y

        LDA TMP_N1
        BEQ AbfcStore0H
        CMP #$01
        BEQ AbfcStore1H
        LDA TMP_N2
        STA WRITE_ALLOC2_LO
        LDA TMP_N3
        STA WRITE_ALLOC2_HI
        JMP AbfcStoredH
AbfcStore0H:
        LDA TMP_N2
        STA WRITE_ALLOC0_LO
        LDA TMP_N3
        STA WRITE_ALLOC0_HI
        JMP AbfcStoredH
AbfcStore1H:
        LDA TMP_N2
        STA WRITE_ALLOC1_LO
        LDA TMP_N3
        STA WRITE_ALLOC1_HI
AbfcStoredH:
        INC TMP_N1
        LDA TMP_N1
        CMP WRITE_NEED_BLOCKS
        BEQ AbfcWriteBitmap

AbfcHiNoAlloc:
        INC TMP_N2
        BNE AbfcHiCandOk
        INC TMP_N3
AbfcHiCandOk:
        LSR TMP_NLEN
        BNE AbfcScanHiBit
        INY
        BNE AbfcScanHiByte

        ; out of allocatable bits in first bitmap block
        LDX #$00
AbfcFullLoop:
        LDA TxtWriteAllocFull,X
        BEQ AbfcFullDone
        JSR Putc
        INX
        BNE AbfcFullLoop
AbfcFullDone:
        SEC
        RTS

AbfcWriteBitmap:
        ; Write updated bitmap block back.
        LDA #CMD_WRITE
        STA pdCommandCode
        LDA #$00
        STA pdUnitNumber
        LDA WRITE_BITMAP_LO
        STA pdBlockNumberLow
        LDA WRITE_BITMAP_HI
        STA pdBlockNumberHigh
        LDA #<READ_VERIFY_BUFFER
        STA pdIOBufferLow
        LDA #>READ_VERIFY_BUFFER
        STA pdIOBufferHigh
        JSR CFBlockDriver
        BCC AbfcOk

        PHA
        LDX #$00
AbfcWrErrLoop:
        LDA TxtWriteAllocErr,X
        BEQ AbfcWrErrDone
        JSR Putc
        INX
        BNE AbfcWrErrLoop
AbfcWrErrDone:
        PLA
        JSR PrintHex
        JSR PrintCR
        SEC
        RTS

AbfcOk:
        CLC
        RTS

;------------------------------------------------------------------------------
; WriteCurrentEntryFromAddr
;
; Input:
;   Source pointer in TMP_N3:TMP_N2
;   Remaining byte length in TMP_E_COUNT:TMP_E_LEN
;   CAT_* metadata populated from catalog entry
;------------------------------------------------------------------------------
WriteCurrentEntryFromAddr:
        LDA CAT_TYPE
        CMP #$01
        BEQ WcaSeedling
        CMP #$02
        BEQ WcaSapling
        LDX #$00
WcaTypeLoop:
        LDA TxtWriteTypeSkip,X
        BEQ WcaTypeDone
        JSR Putc
        INX
        BNE WcaTypeLoop
WcaTypeDone:
        LDA CAT_TYPE
        JSR PrintHex
        JSR PrintCR
        RTS

WcaSeedling:
        ; up to 512 bytes
        LDA TMP_E_COUNT
        CMP #$02
        BCC WcaSeedOk
        BEQ WcaSeedEq2
        JMP WcaTooBig
WcaSeedEq2:
        LDA TMP_E_LEN
        BEQ WcaSeedOk
        JMP WcaTooBig
WcaSeedOk:
        LDA CAT_KEY_LO
        STA TMP_N0
        LDA CAT_KEY_HI
        STA TMP_N1
        JSR StageAndWriteBlock
        BCS WcaSeedWriteErr
        JMP WcaDoneCheck
WcaSeedWriteErr:
        JMP WcaWriteErr

WcaSapling:
        ; up to 1024 bytes
        LDA TMP_E_COUNT
        CMP #$04
        BCC WcaSapSizeOk
        BEQ WcaSapEq0400
        JMP WcaTooBig
WcaSapEq0400:
        LDA TMP_E_LEN
        BEQ WcaSapSizeOk
        JMP WcaTooBig
WcaSapSizeOk:
        ; Read index block from CAT_KEY
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
        BCC WcaIdxOk
        JMP WcaWriteErr

WcaIdxOk:
        ; pointer #0
        LDY #$00
        LDA READ_VERIFY_BUFFER,Y
        STA TMP_N0
        LDA READ_VERIFY_BUFFER+$100,Y
        STA TMP_N1
        ; pointer #1
        LDY #$01
        LDA READ_VERIFY_BUFFER,Y
        STA CAT_AUXLO
        LDA READ_VERIFY_BUFFER+$100,Y
        STA CAT_AUXHI

        ; write block #0 if bytes remain
        LDA TMP_E_LEN
        ORA TMP_E_COUNT
        BNE WcaNeedBlk0
        JMP WcaDoneCheck
WcaNeedBlk0:
        LDA TMP_N0
        ORA TMP_N1
        BEQ WcaIdxErr0
        JSR StageAndWriteBlock
        BCC WcaNeedSecond
        JMP WcaWriteErr

WcaNeedSecond:
        LDA TMP_E_LEN
        ORA TMP_E_COUNT
        BNE WcaNeedBlk1
        JMP WcaDoneCheck
WcaNeedBlk1:

        LDA CAT_AUXLO
        ORA CAT_AUXHI
        BEQ WcaIdxErr1
        LDA CAT_AUXLO
        STA TMP_N0
        LDA CAT_AUXHI
        STA TMP_N1
        JSR StageAndWriteBlock
        BCS WcaSapWriteErr
        JMP WcaDoneCheck
WcaSapWriteErr:
        JMP WcaWriteErr

WcaDoneCheck:
        LDA TMP_E_LEN
        ORA TMP_E_COUNT
        BEQ WcaSuccess
WcaTooBig:
        LDX #$00
WcaBigLoop:
        LDA TxtWriteTooBig,X
        BEQ WcaBigDone
        JSR Putc
        INX
        BNE WcaBigLoop
WcaBigDone:
        RTS

WcaSuccess:
        LDX #$00
WcaOkLoop:
        LDA TxtSuccess,X
        BEQ WcaOkDone
        JSR Putc
        INX
        BNE WcaOkLoop
WcaOkDone:
        JSR PrintCR
        RTS

WcaWriteErr:
        PHA
        LDX #$00
WcaWrErrLoop:
        LDA TxtWriteErr,X
        BEQ WcaWrErrDone
        JSR Putc
        INX
        BNE WcaWrErrLoop
WcaWrErrDone:
        PLA
        JSR PrintHex
        JSR PrintCR
        RTS

WcaIdxErr0:
        LDX #$00
WcaIdx0Loop:
        LDA TxtWriteIdx0Err,X
        BEQ WcaIdx0Done
        JSR Putc
        INX
        BNE WcaIdx0Loop
WcaIdx0Done:
        JSR PrintCR
        RTS

WcaIdxErr1:
        LDX #$00
WcaIdx1Loop:
        LDA TxtWriteIdx1Err,X
        BEQ WcaIdx1Done
        JSR Putc
        INX
        BNE WcaIdx1Loop
WcaIdx1Done:
        JSR PrintCR
        RTS

;------------------------------------------------------------------------------
; StageAndWriteBlock
;
; Inputs:
;   TMP_N0/TMP_N1 = destination block number (lo/hi)
;   TMP_N2/TMP_N3 = source pointer (lo/hi)
;   TMP_E_LEN/TMP_E_COUNT = remaining byte count (lo/hi)
;
; Outputs:
;   Updates TMP_N2/TMP_N3 and TMP_E_LEN/TMP_E_COUNT.
;   CLC on success, SEC/A on write error.
;------------------------------------------------------------------------------
StageAndWriteBlock:
        ; Zero 512-byte staging buffer.
        LDX #$00
SabZero:
        STZ READ_VERIFY_BUFFER,X
        STZ READ_VERIFY_BUFFER+$100,X
        INX
        BNE SabZero

        ; Set source pointer.
        LDA TMP_N2
        STA ZP_PTR_LO
        LDA TMP_N3
        STA ZP_PTR_HI

        ; Copy up to 512 bytes from source into staging buffer.
        LDA TMP_E_COUNT
        CMP #$02
        BCS SabCopy512
        CMP #$01
        BEQ SabCopy256Plus

        ; 0..255 bytes
        LDA TMP_E_LEN
        BEQ SabWrite
        STA TMP_NLEN
        LDY #$00
SabLow:
        LDA (ZP_PTR_LO),Y
        STA READ_VERIFY_BUFFER,Y
        INY
        DEC TMP_NLEN
        BNE SabLow

        ; advance source pointer by low-byte length
        CLC
        LDA TMP_N2
        ADC TMP_E_LEN
        STA TMP_N2
        BCC SabLowNoCarry
        INC TMP_N3
SabLowNoCarry:
        STZ TMP_E_LEN
        STZ TMP_E_COUNT
        JMP SabWrite

SabCopy256Plus:
        ; first 256 bytes
        LDY #$00
SabPg0:
        LDA (ZP_PTR_LO),Y
        STA READ_VERIFY_BUFFER,Y
        INY
        BNE SabPg0

        ; advance source to next page
        INC ZP_PTR_HI

        ; copy remainder from second page (0..255)
        LDA TMP_E_LEN
        BEQ SabAfter256
        STA TMP_NLEN
        LDY #$00
SabPg1Part:
        LDA (ZP_PTR_LO),Y
        STA READ_VERIFY_BUFFER+$100,Y
        INY
        DEC TMP_NLEN
        BNE SabPg1Part
SabAfter256:
        ; source advanced by 256 + low
        LDA TMP_E_LEN
        STA TMP_N1
        LDA TMP_N3
        CLC
        ADC #$01
        STA TMP_N3
        CLC
        LDA TMP_N2
        ADC TMP_N1
        STA TMP_N2
        BCC SabPg1NoCarry
        INC TMP_N3
SabPg1NoCarry:
        STZ TMP_E_LEN
        STZ TMP_E_COUNT
        JMP SabWrite

SabCopy512:
        LDY #$00
SabFull0:
        LDA (ZP_PTR_LO),Y
        STA READ_VERIFY_BUFFER,Y
        INY
        BNE SabFull0
        INC ZP_PTR_HI
        LDY #$00
SabFull1:
        LDA (ZP_PTR_LO),Y
        STA READ_VERIFY_BUFFER+$100,Y
        INY
        BNE SabFull1

        ; source += 512
        LDA TMP_N3
        CLC
        ADC #$02
        STA TMP_N3
        ; remaining -= 512
        SEC
        LDA TMP_E_COUNT
        SBC #$02
        STA TMP_E_COUNT

SabWrite:
        LDA #CMD_WRITE
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
        RTS

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

        BCS MlnCancel
        STA TMP_N0
        JSR PrintCR
        JMP MlnHaveIndex
MlnCancel:
        JSR PrintCR
        JSR PrintCR
        RTS

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
        JSR PrintCR

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

        ; Print entry index (decimal 00..99)
        TXA
        JSR PrintDec2
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
; FindCatalogEntryByIndex - fetch metadata for matching directory slot index
; In:  TMP_N0 = requested entry index (00-99 decimal value)
; Out: C=0 metadata loaded into CAT_*; C=1 if not found / inactive / read error
;------------------------------------------------------------------------------
FindCatalogEntryByIndex:
        JSR ReadCatalogBlock2
        BCC FciReadOk
        SEC
        RTS
FciReadOk:
        LDA TMP_N0
        STA TMP_N1

        LDA READ_VERIFY_BUFFER+$23
        STA TMP_E_LEN
        LDA READ_VERIFY_BUFFER+$24
        STA TMP_E_COUNT

        LDA #<(READ_VERIFY_BUFFER+$2B)
        STA ZP_PTR_LO
        LDA #>(READ_VERIFY_BUFFER+$2B)
        STA ZP_PTR_HI

        LDX #$00
FciEntryLoop:
        LDA TMP_E_COUNT
        BEQ FciNotFound
        CPX TMP_N1
        BEQ FciAtTarget

FciAdvance:
        CLC
        LDA ZP_PTR_LO
        ADC TMP_E_LEN
        STA ZP_PTR_LO
        BCC FciAdvanceNoCarry
        INC ZP_PTR_HI
FciAdvanceNoCarry:
        INX
        DEC TMP_E_COUNT
        JMP FciEntryLoop

FciAtTarget:
        LDY #$00
        LDA (ZP_PTR_LO),Y
        BEQ FciNotFound
        STA TMP_N0
        AND #$0F
        BEQ FciNotFound

        ; Storage type in high nibble.
        LDA TMP_N0
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

FciNotFound:
        SEC
        RTS

;------------------------------------------------------------------------------
; FindCatalogEntryByName - fetch metadata for a matching filename in block 2
; In:  LOAD_NAME_LEN / LOAD_NAME_BUF populated with uppercase filename
; Out: C=0 metadata loaded into CAT_*; C=1 if not found / read error
;------------------------------------------------------------------------------
FindCatalogEntryByName:
        JSR ReadCatalogBlock2
        BCC FcnReadOk
        SEC
        RTS
FcnReadOk:
        LDA READ_VERIFY_BUFFER+$23
        STA TMP_E_LEN
        LDA READ_VERIFY_BUFFER+$24
        STA TMP_E_COUNT

        LDA #<(READ_VERIFY_BUFFER+$2B)
        STA ZP_PTR_LO
        LDA #>(READ_VERIFY_BUFFER+$2B)
        STA ZP_PTR_HI

FcnEntryLoop:
        LDA TMP_E_COUNT
        BNE FcnEntryDo
        SEC
        RTS

FcnEntryDo:
        LDY #$00
        LDA (ZP_PTR_LO),Y
        BEQ FcnAdvance
        STA TMP_N0
        AND #$0F
        BEQ FcnAdvance
        CMP LOAD_NAME_LEN
        BNE FcnAdvance

        STA TMP_N1
        LDY #$01
FcnCmpLoop:
        LDA (ZP_PTR_LO),Y
        CMP LOAD_NAME_BUF-1,Y
        BNE FcnAdvance
        INY
        DEC TMP_N1
        BNE FcnCmpLoop

        ; Storage type in high nibble.
        LDA TMP_N0
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

FcnAdvance:
        CLC
        LDA ZP_PTR_LO
        ADC TMP_E_LEN
        STA ZP_PTR_LO
        BCC FcnAdvanceNoCarry
        INC ZP_PTR_HI
FcnAdvanceNoCarry:
        DEC TMP_E_COUNT
        JMP FcnEntryLoop

;------------------------------------------------------------------------------
; MenuLoadByIndex - load command (00-99 index)
;------------------------------------------------------------------------------
MenuLoadByIndex:
        LDX #$00
MlnPromptLoop:
        LDA TxtLoadFilePrompt,X
        BEQ MlnPromptDone
        JSR Putc
        INX
        BNE MlnPromptLoop
MlnPromptDone:

        JSR ReadDec2OrCR
        BCC MlnHaveIndex_Save
        RTS

MlnHaveIndex_Save:
        STA TMP_N0
        JSR PrintCR

MlnHaveIndex:
        JSR FindCatalogEntryByIndex
        BCC MlnHaveEntry

        LDX #$00
MlnNoFileLoop:
        LDA TxtLoadNoFile,X
        BEQ MlnNoFileDone
        JSR Putc
        INX
        BNE MlnNoFileLoop
MlnNoFileDone:
        RTS

MlnHaveEntry:
        ; BA1 special path intentionally deferred for now.
        LDA CAT_FILETYPE
        CMP #$F1
        BNE MlnNotBa1
        LDX #$00
MlnBa1Loop:
        LDA TxtLoadBa1NYI,X
        BEQ MlnBa1Done
        JSR Putc
        INX
        BNE MlnBa1Loop
MlnBa1Done:
        RTS

MlnNotBa1:
        ; Default addr from auxtype.
        LDA CAT_AUXLO
        STA TMP_N2
        LDA CAT_AUXHI
        STA TMP_N3

        LDX #$00
MlnAddrLoop:
        LDA TxtAddrPromptA,X
        BEQ MlnAddrA_Done
        JSR Putc
        INX
        BNE MlnAddrLoop
MlnAddrA_Done:
        LDA TMP_N3
        JSR PrintHex
        LDA TMP_N2
        JSR PrintHex
        LDX #$00
MlnAddrB_Loop:
        LDA TxtAddrPromptB,X
        BEQ MlnAddrB_Done
        JSR Putc
        INX
        BNE MlnAddrB_Loop
MlnAddrB_Done:

        ; Enter keeps default; otherwise 4 hex digits override.
        JSR GetHexNibbleOrCR
        BCC MlnAddrN0
        JMP MlnDoLoad
MlnAddrN0:
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

MlnDoLoad:
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
; ReadFilenameOrCR - read uppercase filename (max 15 chars), echoing input
; Returns: C=1 if CR pressed immediately, else C=0 and LOAD_NAME_* populated
;------------------------------------------------------------------------------
ReadFilenameOrCR:
        LDA #$00
        STA LOAD_NAME_LEN
ReadNameLoop:
        JSR GetKey
        CMP #$0D
        BEQ ReadNameDone
        JSR ToUpper
        CMP #$21
        BCC ReadNameLoop
        LDX LOAD_NAME_LEN
        CPX #$0F
        BCS ReadNameLoop
        STA LOAD_NAME_BUF,X
        PHX
        JSR Putc
        PLX
        INC LOAD_NAME_LEN
        JMP ReadNameLoop
ReadNameDone:
        LDA LOAD_NAME_LEN
        BNE ReadNameOk
        SEC
        RTS
ReadNameOk:
        CLC
        RTS

;------------------------------------------------------------------------------
; ReadDec2OrCR - read exactly two decimal digits (00..99), echoing input
; Returns: C=1 if CR pressed at first char, else C=0 and A=index (00..99)
;------------------------------------------------------------------------------
ReadDec2OrCR:
Rd2First:
        JSR GetKey
        CMP #$0D
        BEQ Rd2Exit
        CMP #$30
        BCC Rd2First
        CMP #$3A
        BCS Rd2First
        PHA
        JSR Putc
        PLA
        SEC
        SBC #$30
        STA TMP_N0

Rd2Second:
        JSR GetKey
        CMP #$30
        BCC Rd2Second
        CMP #$3A
        BCS Rd2Second
        PHA
        JSR Putc
        PLA
        SEC
        SBC #$30
        STA TMP_N1

        ; A = (tens * 10) + ones
        LDA TMP_N0
        ASL A
        STA TMP_N2           ; 2*tens
        ASL A                ; 4*tens
        ASL A                ; 8*tens
        CLC
        ADC TMP_N2           ; 10*tens
        CLC
        ADC TMP_N1
        CLC
        RTS

Rd2Exit:
        SEC
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
; PrintDec2 - print A (0..99) as two decimal digits
;------------------------------------------------------------------------------
PrintDec2:
        PHX
        LDX #$00
Pd2Loop:
        CMP #$0A
        BCC Pd2Done
        SEC
        SBC #$0A
        INX
        JMP Pd2Loop
Pd2Done:
        PHA
        TXA
        CLC
        ADC #$30
        JSR Putc
        PLA
        CLC
        ADC #$30
        JSR Putc
        PLX
        RTS

;------------------------------------------------------------------------------
; Strings
;------------------------------------------------------------------------------
TxtBanner:
        .byte $0D
        .asciiz "NEO1 CFFA1 M8.2 WRITE FILE"
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
        .asciiz "C L B W D Q ?"
TxtLoadFilePrompt:
        .byte $0D
        .asciiz "LOAD IDX (00-99): "
TxtLoadNoFile:
        .byte $0D
        .asciiz "LOAD ERR:EMPTY IDX"
TxtLoadBa1NYI:
        .byte $0D
        .asciiz "LOAD BA1 NYI"
TxtWriteFrom:
        .byte $0D
        .asciiz "WRITE FROM: $"
TxtWriteLen:
        .byte $0D
        .asciiz " LENGTH: $"
TxtWriteType:
        .byte $0D
        .asciiz "TYPE (BIN): $"
TxtWriteName:
        .byte $0D
        .asciiz "      NAME: "
TxtDeletePrompt:
        .byte $0D
        .asciiz "DELETE IDX (00-99): "
TxtDeleteNoFile:
        .byte $0D
        .asciiz "DELETE ERR:EMPTY IDX"
TxtDeleteErr:
        .byte $0D
        .asciiz "DELETE ERR:"
TxtDeleteOk:
        .byte $0D
        .asciiz "DELETE OK"
TxtWriteNoFile:
        .byte $0D
        .asciiz "WRITE ERR:NO FILE"
TxtWriteDirErr:
        .byte $0D
        .asciiz "WRITE ERR:DIR:"
TxtWriteDirFull:
        .byte $0D
        .asciiz "WRITE ERR:DIR FULL"
TxtWriteAllocErr:
        .byte $0D
        .asciiz "WRITE ERR:ALLOC:"
TxtWriteAllocFull:
        .byte $0D
        .asciiz "WRITE ERR:ALLOC FULL"
TxtWriteErr:
        .byte $0D
        .asciiz "WRITE ERR:"
TxtWriteTypeErr:
        .byte $0D
        .asciiz "WRITE ERR:TYPE="
TxtWriteTypeSkip:
        .byte $0D
        .asciiz "WRITE SKIP:TYPE="
TxtWriteTooBig:
        .byte $0D
        .asciiz "WRITE SKIP:LEN>ALLOC"
TxtWriteIdx0Err:
        .byte $0D
        .asciiz "WRITE ERR:SAPLING IDX0"
TxtWriteIdx1Err:
        .byte $0D
        .asciiz "WRITE ERR:SAPLING IDX1"
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
