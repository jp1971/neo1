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

KBD          = $D010
KBDCR        = $D011
DSP          = $D012

CMD_DIR_OPEN = $10
CMD_DIR_NEXT = $11
CMD_OPEN_IND = $12
CMD_READ     = $03
INFO_VALID   = $01

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

        .org $C100

;------------------------------------------------------------------------------
; VaciMain - VACI entry point at $C100
;------------------------------------------------------------------------------
VaciMain:
        ; Print banner
        LDA #$0D
        JSR Putc
        
        LDX #$00
BannerLoop:
        LDA TxtBanner,X
        BEQ BannerDone
        JSR Putc
        INX
        BNE BannerLoop
BannerDone:
        JSR PrintCR

        ; Main menu loop
MenuLoop:
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
        JSR PrintCR
        
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
        LDX #$00
WriteDeferLoop:
        LDA TxtWriteDefer,X
        BEQ WriteDeferDone
        JSR Putc
        INX
        BNE WriteDeferLoop
WriteDeferDone:
        JSR PrintCR
        JMP MenuLoop

MenuQuit:
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
        JSR PrintStarSpace
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
        JSR PrintVaciPrefix
        
        ; Display ACI command: "HHLL . HHLLR"
        LDA ZP_ADDR_HI
        JSR PrintHex
        LDA ZP_ADDR_LO
        JSR PrintHex
        
        LDX #$00
AciCmdLoop:
        LDA TxtAciCmd,X
        BEQ AciCmdDone
        JSR Putc
        INX
        BNE AciCmdLoop
        
AciCmdDone:
        LDA ZP_ADDR_HI
        JSR PrintHex
        LDA ZP_ADDR_LO
        JSR PrintHex
        
        LDA #'R'
        JSR Putc
        JSR PrintCR
        
        ; Wait for CR to execute
        JSR GetKey
        CMP #$0D
        BNE VrExecuteCancel
        JSR PrintCR
        
        ; Open file by index
        LDA ZP_INDEX
        STA MSC_INDEX
        LDA #CMD_OPEN_IND
        STA MSC_CMD
        JSR WaitReady
        BCC VrFileOpen
        RTS
        
VrFileOpen:
        ; Read file (512 bytes = 2 pages at 256 bytes each)
        LDA #$00
        STA MSC_SECT_LO
        STA MSC_SECT_HI
        LDA #CMD_READ
        STA MSC_CMD
        JSR WaitReady
        BCC VrReadOk
        RTS
        
VrReadOk:
        ; Copy 512 bytes from MSC_DATA to destination
        LDA ZP_ADDR_LO
        STA ZP_PTR_LO
        LDA ZP_ADDR_HI
        STA ZP_PTR_HI
        
        ; First 256 bytes (Y = 0..255)
        LDY #$00
CopyPage0:
        LDA MSC_DATA
        STA (ZP_PTR_LO),Y
        INY
        BNE CopyPage0
        
        ; Second 256 bytes
        INC ZP_PTR_HI
        LDY #$00
CopyPage1:
        LDA MSC_DATA
        STA (ZP_PTR_LO),Y
        INY
        BNE CopyPage1
        
        ; Success
        LDX #$00
SuccessLoop:
        LDA TxtSuccess,X
        BEQ SuccessDone
        JSR Putc
        INX
        BNE SuccessLoop
        
SuccessDone:
        JSR PrintCR
        RTS
        
VrIndexCancel:
VrAddrCancel:
VrExecuteCancel:
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
TxtBanner:
        .asciiz "* NEO1 VACI V1"

TxtPrompt:
        .byte $0D
        .asciiz "* R/W/Q? "

TxtIdxPrompt:
        .byte $0D
        .asciiz "* INDEX (00-99): "

TxtAddrPrompt:
        .byte $0D
        .asciiz "* ADDR ($XXXX): "

TxtAciCmd:
        .asciiz " . "

TxtSuccess:
        .asciiz "* READ OK"

TxtWriteDefer:
        .byte $0D
        .asciiz "* WRITE: DEFERRED (V2)"
