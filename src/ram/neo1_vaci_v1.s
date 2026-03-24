; Neo1 Virtual Apple Cassette Interface (VACI) — V1 Read Flow
;
; Entry: C100R from WozMon
; Menu:  R = read cassette (by index 00-99) to address
;        W = write cassette (deferred V2)
;        Q = quit to WozMon
;
; ACI-style flow:
;   - user selects file index
;   - user enters load address
;   - display synthesized command (e.g., "0300 . 0328R")
;   - user presses CR to execute transfer
;   - return to WozMon on success
;
; MSC backend registers:
;   $D014  MSC_CMD      (command write)
;   $D015  MSC_SECT_LO  (sector/block low)
;   $D016  MSC_SECT_HI  (sector/block high)
;   $D017  MSC_DATA     (data stream read/write)
;   $D018  MSC_STATUS   (status/ready) 
;   $D019  MSC_INDEX    (file index for open-by-index)
;   $D01A  MSC_INFO     (info flags)
;
; I/O:
;   $D010  KBD          (keyboard data)
;   $D011  KBDCR        (keyboard control/strobe)
;   $D012  DSP          (display/UART)

.setcpu "65C02"

.export VaciMain

MSC_CMD      = $D014
MSC_SECT_LO  = $D015
MSC_SECT_HI  = $D016
MSC_DATA     = $D017
MSC_STATUS   = $D018
MSC_INDEX    = $D019
MSC_INFO     = $D01A
MSC_SIZE_LO  = $D01B
MSC_SIZE_HI  = $D01C

KBD          = $D010
KBDCR        = $D011
DSP          = $D012

CMD_OPEN     = $01
CMD_DIR_OPEN = $10
CMD_DIR_NEXT = $11
CMD_OPEN_IND = $12
CMD_CLOSE    = $02
CMD_READ     = $03
CMD_WRITE    = $04
INFO_VALID   = $01

; Filename buffer in page 2 (writable, WozMon input area — safe while VACI runs)
VACI_FNAME_BUF = $0200

WOZMON_ENTRY = $FF00

; Zero page temporaries
ZP_PTR_LO    = $F0
ZP_PTR_HI    = $F1
ZP_ADDR_LO   = $F2
ZP_ADDR_HI   = $F3
ZP_INDEX     = $F4
ZP_B0        = $F5
ZP_B1        = $F6
ZP_TEMPLO    = $F7
ZP_TEMPHI    = $F8
ZP_TENS      = $F9
ZP_ONES      = $FA
ZP_END_LO    = $FB
ZP_END_HI    = $FC

        .org $C100

;------------------------------------------------------------------------------
; VaciMain - VACI entry point at $C100
;------------------------------------------------------------------------------
VaciMain:
        ; Print '*' immediately — sit on WozMon's "C100: A9" line
        LDA #'*'
        JSR Putc

        ; Main menu loop
MenuLoop:
        JSR PrintCR
        LDX #$00
PromptLoop:
        LDA TxtPrompt,X
        BEQ PromptDone
        JSR Putc
        INX
        BNE PromptLoop
PromptDone:
        JSR GetKey
        JSR ToUpper
        PHA
        JSR Putc
        PLA
        CMP #'Q'
        BEQ MenuQuit
        PHA
        JSR PrintCR
        PLA
        
        CMP #'R'
        BEQ MenuRead
        CMP #'W'
        BEQ MenuWrite
        
        ; Unknown command, loop
        JMP MenuLoop

MenuRead:
        JSR VaciRead
        JMP MenuLoop

MenuWrite:
        JSR VaciWrite
        JMP MenuLoop

MenuQuit:
        JSR PrintCR
        JMP WOZMON_ENTRY

;------------------------------------------------------------------------------
; VaciRead - Read a cassette file by index
;
; Flow:
;   1. Enumerate files (DIR_OPEN, then DIR_NEXT loop)
;   2. Prompt for index (00-99)
;   3. Prompt for load address ($XXXX)
;   4. Display ACI-style command echo
;   5. Wait for CR to execute
;   6. Open file by index, read first 512 bytes, copy to destination
;   7. Return to caller
;------------------------------------------------------------------------------
VaciRead:
        ; Initialize index counter
        LDA #$00
        STA ZP_INDEX

        ; Open directory
        LDA #CMD_DIR_OPEN
        STA MSC_CMD
        JSR WaitReady
        BCC VrOpenOk
        RTS

VrOpenOk:
        ; List files
ListLoop:
        LDA ZP_INDEX
        CMP #$64            ; 100 files max
        BCS ListDone
        
        LDA #CMD_DIR_NEXT
        STA MSC_CMD
        JSR WaitReady
        BCC VrNextOk
        RTS
        
VrNextOk:
        LDA MSC_INFO
        AND #INFO_VALID
        BEQ ListDone
        
        ; Print index (00-99)
        LDA ZP_INDEX
        JSR PrintDec2
        LDA #$3A            ; ':'
        JSR Putc
        LDA #$20            ; space
        JSR Putc
        
        ; Print filename from MSC_DATA stream
PrintNameLoop:
        LDA MSC_DATA
        BEQ NameDone
        JSR Putc
        JMP PrintNameLoop
        
NameDone:
        JSR PrintCR
        INC ZP_INDEX
        JMP ListLoop
        
ListDone:
        ; Prompt for index
        LDX #$00
IdxPromptLoop:
        LDA TxtIdxPrompt,X
        BNE IdxPromptNext
        JMP IdxPromptDone
IdxPromptNext:
        JSR Putc
        INX
        JMP IdxPromptLoop
        
IdxPromptDone:
        JSR ReadDec2
        BCC VrIndexOk
        JMP VrIndexCancel
VrIndexOk:
        STA ZP_INDEX
        JSR PrintCR

        ; Open selected file now so size metadata is available for range echo
        LDA ZP_INDEX
        STA MSC_INDEX
        LDA #CMD_OPEN_IND
        STA MSC_CMD
        JSR WaitReady
        BCC VrOpenIndexOk
        RTS

VrOpenIndexOk:
        
        ; Prompt for address
        LDX #$00
AddrPromptLoop:
        LDA TxtAddrPrompt,X
        BNE AddrPromptNext
        JMP AddrPromptDone
AddrPromptNext:
        JSR Putc
        INX
        JMP AddrPromptLoop
        
AddrPromptDone:
        JSR ReadHexWord
        BCC VrAddrOk
        JMP VrAddrCancel
VrAddrOk:
        ; ZP_ADDR_HI:ZP_ADDR_LO already set by ReadHexWord

        ; Compute end address for ACI-style echo:
        ;   end = start + (file_size - 1), if size>0
        LDA ZP_ADDR_LO
        STA ZP_END_LO
        LDA ZP_ADDR_HI
        STA ZP_END_HI

        LDA MSC_SIZE_LO
        STA ZP_B0
        LDA MSC_SIZE_HI
        STA ZP_B1

        LDA ZP_B0
        ORA ZP_B1
        BEQ VrEndReady

        ; size = size - 1
        LDA ZP_B0
        BNE VrDecLenLo
        DEC ZP_B1
VrDecLenLo:
        DEC ZP_B0

        ; end += (size - 1)
        CLC
        LDA ZP_END_LO
        ADC ZP_B0
        STA ZP_END_LO
        LDA ZP_END_HI
        ADC ZP_B1
        STA ZP_END_HI

VrEndReady:
        JSR PrintCR
        
        ; Display ACI command: "HHLL.HHLLR"
        LDA ZP_ADDR_HI
        JSR PrintHex
        LDA ZP_ADDR_LO
        JSR PrintHex
        
        LDX #$00
AciCmdLoop:
        LDA TxtReadAciCmd,X
        BEQ AciCmdDone
        JSR Putc
        INX
        BNE AciCmdLoop
        
AciCmdDone:
        LDA ZP_END_HI
        JSR PrintHex
        LDA ZP_END_LO
        JSR PrintHex
        
        LDA #'R'
        JSR Putc

        ; Read file (512 bytes = 2 pages at 256 bytes each)
        LDA #$00
        STA MSC_SECT_LO
        STA MSC_SECT_HI
        LDA #CMD_READ
        STA MSC_CMD
        JSR WaitReady
        BCC VrReadOk
        LDA #CMD_CLOSE
        STA MSC_CMD
        JSR WaitReady
        RTS
        
VrReadOk:
        ; Copy exactly file_size bytes from MSC_DATA to destination.
        LDA ZP_ADDR_LO
        STA ZP_PTR_LO
        LDA ZP_ADDR_HI
        STA ZP_PTR_HI

        LDA MSC_SIZE_LO
        STA ZP_B0
        LDA MSC_SIZE_HI
        STA ZP_B1

        LDA ZP_B0
        ORA ZP_B1
        BEQ VrCopyDone
        
VrFullPageCheck:
        LDA ZP_B1
        BEQ VrPartial
        LDY #$00
VrCopyPage:
        LDA MSC_DATA
        STA (ZP_PTR_LO),Y
        INY
        BNE VrCopyPage
        INC ZP_PTR_HI
        DEC ZP_B1
        JMP VrFullPageCheck

VrPartial:
        LDA ZP_B0
        BEQ VrCopyDone
        STA ZP_TEMPLO
        LDY #$00
VrCopyTail:
        LDA MSC_DATA
        STA (ZP_PTR_LO),Y
        DEC ZP_TEMPLO
        BEQ VrCopyDone
        INY
        JMP VrCopyTail

VrCopyDone:
        JSR PrintCR
        JMP WOZMON_ENTRY
        
VrIndexCancel:
        RTS

VrAddrCancel:
        LDA #CMD_CLOSE
        STA MSC_CMD
        JSR WaitReady
        RTS

;------------------------------------------------------------------------------
; VaciWrite - Write a RAM region to a named file on USB (ACI-style)
;
; Flow:
;   1. Prompt "* FILENAME: " - read up to 15 chars into VACI_FNAME_BUF
;   2. Prompt "* START ($XXXX): " - ReadHexWord -> start addr (ZP_PTR)
;   3. Prompt "* END ($XXXX): "   - ReadHexWord -> end addr  (ZP_END)
;   4. Compute length = end - start + 1, cap at 512
;   5. Echo "* AAAA . EEEEW", wait for CR (other key = cancel)
;   6. CMD_OPEN, stream filename+NUL to MSC_DATA (triggers do_open())
;   7. WaitReady; stream ZP_B1:ZP_B0 payload bytes, pad remainder to 512
;   8. Set sector 0, CMD_WRITE, WaitReady
;   9. CMD_CLOSE, WaitReady
;  10. Print "* WRITE OK", RTS
;
; ZP usage: ZP_PTR_LO/HI=start, ZP_END_LO/HI=end, ZP_B0/B1=length(lo/hi)
;------------------------------------------------------------------------------
VaciWrite:
        ; ---- Filename prompt ----
        LDX #$00
VwFnPrLoop:
        LDA TxtWrFnamePrompt,X
        BEQ VwFnPrDone
        JSR Putc
        INX
        JMP VwFnPrLoop
VwFnPrDone:
        ; Read up to 15 chars (uppercase), stop on CR, NUL-terminate
        LDX #$00
VwFnReadLoop:
        CPX #$0F
        BCS VwFnEnd
        JSR GetKey
        CMP #$0D
        BEQ VwFnEnd
        JSR ToUpper
        JSR Putc
        STA VACI_FNAME_BUF,X
        INX
        JMP VwFnReadLoop
VwFnEnd:
        TXA
        BNE VwFnNotEmpty
        JMP VwCancel            ; empty filename: cancel
VwFnNotEmpty:
        LDA #$00
        STA VACI_FNAME_BUF,X   ; NUL-terminate

        ; ---- Start address ----
        LDX #$00
VwStartPrLoop:
        LDA TxtWrStartPrompt,X
        BEQ VwStartPrDone
        JSR Putc
        INX
        JMP VwStartPrLoop
VwStartPrDone:
        JSR ReadHexWord
        BCC VwStartOk
        JMP VwCancel
VwStartOk:
        LDA ZP_ADDR_LO
        STA ZP_PTR_LO
        LDA ZP_ADDR_HI
        STA ZP_PTR_HI

        ; ---- End address ----
        LDX #$00
VwEndPrLoop:
        LDA TxtWrEndPrompt,X
        BEQ VwEndPrDone
        JSR Putc
        INX
        JMP VwEndPrLoop
VwEndPrDone:
        JSR ReadHexWord
        BCC VwEndOk
        JMP VwCancel
VwEndOk:
        LDA ZP_ADDR_LO
        STA ZP_END_LO
        LDA ZP_ADDR_HI
        STA ZP_END_HI

        ; ---- Compute length = (end - start) + 1, cap at 512 ($0200) ----
        SEC
        LDA ZP_END_LO
        SBC ZP_PTR_LO
        STA ZP_B0
        LDA ZP_END_HI
        SBC ZP_PTR_HI
        STA ZP_B1
        ; +1
        INC ZP_B0
        BNE VwLenNoCarry
        INC ZP_B1
VwLenNoCarry:
        ; Cap: if ZP_B1 > 2, or ZP_B1 == 2 and ZP_B0 > 0 -> length > 512
        LDA ZP_B1
        CMP #$02
        BCC VwLenOk
        BEQ VwChkB0
        JMP VwCapLen
VwChkB0:
        LDA ZP_B0
        BEQ VwLenOk
VwCapLen:
        LDA #$02
        STA ZP_B1
        LDA #$00
        STA ZP_B0
VwLenOk:
        ; ---- ACI echo: CR "AAAA.EEEEW" ----
        JSR PrintCR
        LDA ZP_PTR_HI
        JSR PrintHex
        LDA ZP_PTR_LO
        JSR PrintHex
        LDX #$00
VwAciMidLoop:
        LDA TxtReadAciCmd,X
        BEQ VwAciMidDone
        JSR Putc
        INX
        JMP VwAciMidLoop
VwAciMidDone:
        LDA ZP_END_HI
        JSR PrintHex
        LDA ZP_END_LO
        JSR PrintHex
        LDA #'W'
        JSR Putc

        ; ---- Auto execute immediately after echo ----
VwDoExecute:

        ; ---- Open file: CMD_OPEN then stream filename+NUL to MSC_DATA ----
        ; Writing to MSC_DATA during OPEN fills g_open_filename; the NUL byte
        ; triggers do_open() which (after our g_data_offset=0 fix) leaves the
        ; write buffer clean and ready.
        LDA #CMD_OPEN
        STA MSC_CMD
        LDX #$00
VwOpenFnLoop:
        LDA VACI_FNAME_BUF,X
        STA MSC_DATA
        BEQ VwOpenFnDone        ; NUL sent -> do_open() triggered internally
        INX
        CPX #$10                ; safety: 16-byte limit
        BCC VwOpenFnLoop
        LDA #$00                ; force NUL after 16 bytes
        STA MSC_DATA
VwOpenFnDone:
        JSR WaitReady
        BCC VwOpenOk
        JMP VwWriteErr          ; open failed
VwOpenOk:
        ; Tell backend how many bytes are intentionally valid for this write.
        LDA ZP_B0
        STA MSC_SIZE_LO
        LDA ZP_B1
        STA MSC_SIZE_HI

        ; ---- Stream payload to MSC_DATA (page 0 then page 1 of sector) ----
        ; ZP_B1 = full 256-byte pages from RAM (0, 1, or 2)
        ; ZP_B0 = remaining bytes in partial last page (0-255)
        ; ZP_PTR = source address

        ; Page 0
        LDY #$00
        LDA ZP_B1
        BEQ VwPg0Partial        ; ZP_B1==0: only ZP_B0 bytes in page 0
        ; ZP_B1>=1: full 256 bytes from RAM
VwPg0Full:
        LDA (ZP_PTR_LO),Y
        STA MSC_DATA
        INY
        BNE VwPg0Full
        INC ZP_PTR_HI
        JMP VwPage1

VwPg0Partial:
        LDA ZP_B0
        BEQ VwPg0PadAll         ; zero bytes: pad entire page 0
        STA ZP_TEMPLO
VwPg0PayLoop:
        LDA (ZP_PTR_LO),Y
        STA MSC_DATA
        INY
        DEC ZP_TEMPLO
        BNE VwPg0PayLoop
        ; Pad page 0 remainder with zeros (Y..255)
        LDA #$00
VwPg0PadRestLoop:
        STA MSC_DATA
        INY
        BNE VwPg0PadRestLoop
        JMP VwPage1

VwPg0PadAll:
        LDA #$00
VwPg0PadAllLoop:
        STA MSC_DATA
        INY
        BNE VwPg0PadAllLoop
        ; fall through to VwPage1

        ; Page 1
VwPage1:
        LDY #$00
        LDA ZP_B1
        CMP #$02
        BEQ VwPg1Full           ; 2 full pages: both from RAM
        CMP #$01
        BEQ VwPg1Partial        ; 1 full page: page 1 has ZP_B0 bytes + pad
        ; ZP_B1==0: page 1 all zeros
VwPg1PadAll:
        LDA #$00
VwPg1PadAllLoop:
        STA MSC_DATA
        INY
        BNE VwPg1PadAllLoop
        JMP VwDataDone

VwPg1Full:
        LDA (ZP_PTR_LO),Y
        STA MSC_DATA
        INY
        BNE VwPg1Full
        JMP VwDataDone

VwPg1Partial:
        LDA ZP_B0
        BEQ VwPg1PadAll         ; ZP_B1==1, ZP_B0==0 means exactly 256 bytes
        STA ZP_TEMPLO
VwPg1PayLoop:
        LDA (ZP_PTR_LO),Y
        STA MSC_DATA
        INY
        DEC ZP_TEMPLO
        BNE VwPg1PayLoop
        LDA #$00
VwPg1PadRestLoop:
        STA MSC_DATA
        INY
        BNE VwPg1PadRestLoop
        JMP VwDataDone

VwDataDone:
        ; ---- CMD_WRITE sector 0 ----
        LDA #$00
        STA MSC_SECT_LO
        STA MSC_SECT_HI
        LDA #CMD_WRITE
        STA MSC_CMD
        JSR WaitReady
        BCC VwWriteOk
        ; Write failed: attempt close then report error
        PHA
        LDA #CMD_CLOSE
        STA MSC_CMD
        JSR WaitReady
        PLA
        JMP VwWriteErr

VwWriteOk:
        ; ---- CMD_CLOSE ----
        LDA #CMD_CLOSE
        STA MSC_CMD
        JSR WaitReady
        BCS VwWriteErr

        ; ---- Success: return quietly ----
        JSR PrintCR
        JMP WOZMON_ENTRY

VwWriteErr:
        LDX #$00
VwErLoop:
        LDA TxtWrError,X
        BEQ VwErDone
        JSR Putc
        INX
        JMP VwErLoop
VwErDone:
        JSR PrintCR
        RTS

VwCancel:
        JSR PrintCR
        RTS

;------------------------------------------------------------------------------
; ReadDec2 - Read two decimal digits, return value in A (0-99)
; Returns: C=1 if CR, C=0 and A=value if two digits read
;------------------------------------------------------------------------------
ReadDec2:
        JSR GetKey
        CMP #$0D
        BEQ Rd2Cancel
        JSR ToUpper
        JSR Putc
        SEC
        SBC #$30
        CMP #$0A
        BCC Rd2D1Ok
        JMP ReadDec2
        
Rd2D1Ok:
        STA ZP_TENS
        
        JSR GetKey
        JSR Putc
        SEC
        SBC #$30
        CMP #$0A
        BCC Rd2D2Ok
        JMP ReadDec2
        
Rd2D2Ok:
        STA ZP_ONES
        
        ; value = (tens * 10) + ones = (tens * 8 + tens * 2) + ones
        LDA ZP_TENS
        ASL A
        STA ZP_TEMPLO
        ASL A
        ASL A
        CLC
        ADC ZP_TEMPLO
        CLC
        ADC ZP_ONES
        CLC
        RTS
        
Rd2Cancel:
        SEC
        RTS

;------------------------------------------------------------------------------
; ReadHexWord - Read 4 hex digits (HHLL), accumulate into 16-bit value
; Returns: C=0 on success with value in ZP_ADDR_HI:ZP_ADDR_LO
;          C=1 if invalid character (user must retry)
;------------------------------------------------------------------------------
ReadHexWord:
        LDX #$00            ; nibble counter (0-3)
        LDA #$00
        STA ZP_ADDR_HI
        STA ZP_ADDR_LO
        
RhwLoop:
        JSR GetKey
        CMP #$0D
        BEQ RhwCancel
        
        JSR Putc
        JSR HexToNibble
        BCS ReadHexWord     ; invalid digit, restart sequence
        
        ; Shift accumulator left 4 bits and insert new nibble
        ; If count < 2: accumulate into HI byte
        ; If count >= 2: accumulate into LO byte
        
        CPX #$02
        BCS RhwLoByte
        
        ; High byte: already have HI in A at start of iteration
        STA ZP_TEMPLO
        LDA ZP_ADDR_HI
        ASL A
        ASL A
        ASL A
        ASL A
        ORA ZP_TEMPLO
        STA ZP_ADDR_HI
        JMP RhwNext
        
RhwLoByte:
        STA ZP_TEMPLO
        LDA ZP_ADDR_LO
        ASL A
        ASL A
        ASL A
        ASL A
        ORA ZP_TEMPLO
        STA ZP_ADDR_LO
        
RhwNext:
        INX
        CPX #$04
        BEQ RhwDone
        JMP RhwLoop
RhwDone:
        
        CLC
        RTS
        
RhwCancel:
        SEC
        RTS

;------------------------------------------------------------------------------
; HexToNibble - Convert ASCII hex character to nibble (0-F)
; Returns: C=0 and A=nibble, or C=1 if invalid
;------------------------------------------------------------------------------
HexToNibble:
        CMP #'0'
        BCC HtnInvalid
        CMP #'9' + 1
        BCC HtnDigit
        CMP #'A'
        BCC HtnInvalid
        CMP #'F' + 1
        BCC HtnAlpha
        CMP #'a'
        BCC HtnInvalid
        CMP #'f' + 1
        BCC HtnLAlpha
        JMP HtnInvalid
        
HtnDigit:
        SEC
        SBC #'0'
        CLC
        RTS
        
HtnAlpha:
        SEC
        SBC #'A' - $0A
        CLC
        RTS
        
HtnLAlpha:
        SEC
        SBC #'a' - $0A
        CLC
        RTS
        
HtnInvalid:
        SEC
        RTS

;------------------------------------------------------------------------------
; PrintHex - Print byte in A as two hex digits
;------------------------------------------------------------------------------
PrintHex:
        PHA
        LSR A
        LSR A
        LSR A
        LSR A
        JSR HexDigit
        PLA
        AND #$0F
        
HexDigit:
        CMP #$0A
        BCC HdDigit
        CLC
        ADC #$07
        
HdDigit:
        CLC
        ADC #$30
        JMP Putc

;------------------------------------------------------------------------------
; PrintDec2 - Print A (0-99) as two decimal digits
;------------------------------------------------------------------------------
PrintDec2:
        PHA
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
        PLA
        RTS

;------------------------------------------------------------------------------
; PrintCR - Print carriage return
;------------------------------------------------------------------------------
PrintCR:
        LDA #$0D
        JMP Putc

;------------------------------------------------------------------------------
; PrintStarSpace - Print "* "
;------------------------------------------------------------------------------
PrintStarSpace:
        LDA #'*'
        JSR Putc
        LDA #$20
        JMP Putc

;------------------------------------------------------------------------------
; PrintVaciPrefix - New line + "* "
;------------------------------------------------------------------------------
PrintVaciPrefix:
        JSR PrintCR
        JSR PrintStarSpace
        RTS

;------------------------------------------------------------------------------
; ToUpper - Convert A to uppercase (if A-Z range, already upper)
;------------------------------------------------------------------------------
ToUpper:
        CMP #'a'
        BCC TuDone
        CMP #'z' + 1
        BCS TuDone
        SEC
        SBC #$20
        
TuDone:
        RTS

;------------------------------------------------------------------------------
; WaitReady - Poll MSC_STATUS until ready
; STATUS: $00=BUSY, $01=READY, $80+=ERROR
; Returns: C=0 if ready, C=1 if error
;------------------------------------------------------------------------------
WaitReady:
WrPoll:
        LDA MSC_STATUS
        BEQ WrPoll          ; $00 = BUSY, keep polling
        BPL WrOk            ; bit 7 clear = READY ($01..$7F)
        SEC                 ; bit 7 set   = ERROR ($80+)
        RTS
WrOk:
        CLC
        RTS

;------------------------------------------------------------------------------
; Putc - Write character to display
;------------------------------------------------------------------------------
Putc:
        STA DSP
        RTS

;------------------------------------------------------------------------------
; GetKey - Wait for keyboard, return ASCII in A (bit 7 stripped)
;------------------------------------------------------------------------------
GetKey:
GkWait:
        LDA KBDCR
        BPL GkWait
        LDA KBD
        AND #$7F
        RTS

;------------------------------------------------------------------------------
; Strings
;------------------------------------------------------------------------------
TxtPrompt:
        .asciiz "R/W/Q?: "

TxtIdxPrompt:
        .byte $0D, $22, "CASSETTE", $22, " (00-99): ", $00

TxtAddrPrompt:
        .byte $0D
        .asciiz "START ($XXXX): "

TxtReadAciCmd:
        .asciiz "."

TxtAciCmd:
        .asciiz " . "

TxtWrFnamePrompt:
        .byte $0D
        .asciiz "FILENAME: "

TxtWrStartPrompt:
        .byte $0D
        .asciiz "START ($XXXX): "

TxtWrEndPrompt:
        .byte $0D
        .asciiz "END ($XXXX): "

TxtWrError:
        .asciiz "WRITE ERR"
