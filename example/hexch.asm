;;; Type 2-byte hex code directly to enter characters.
;;; Type 'A''A' in succession, for example, to input
;;; CHRS(&hAA). ESC to triggger, '00' to exit the mode.
;;; Non-hex code is ignored and gives a beep. Cursor
;;; movement is allowed.
;;;
;;; Specifies the entry point(13B0h) to auto-start
;;; the program after the loading:
;;;
;;; SAVEM "HEXCH", &HFF00, &HFF28, &H13B0
;;;
COLORF: EQU 1180h
ASCGET: EQU 0C62h
CURTM:  EQU 0D22h
ACCPRT: EQU 0864h
BHEXIN: EQU 0540h
HEX2IN: EQU 052Ch
BELL:   EQU 06CAh
DEPRT:  EQU 07F3h
ESC:    EQU 1175h

ORG 0FE80h
HEXCH:
    XOR A
    LD (CURTM),A
    LD DE,HEXBUF
HEXBUFL:
    CALL ASCGET
    LD (DE),A
    CALL CKCURS
    JR C,HEXCHO
    INC DE
    CALL ASCGET
    LD (DE),A
    DEC DE
    CALL HEX2IN
    JR C,HEXINPW
    OR A
    JR Z,HEXINQ
HEXCHO:
    CALL ACCPRT
    JR HEXBUFL
HEXINPW:
    CALL BELL
    JR HEXBUFL
CKCURS:
    CP 1Ch                      ;1Ch~1Fh arrow keys
    CCF
    RET NC
    CP 20h
    RET
HEXINQ:
    LD A,16
    LD (CURTM),A
    RET
HEXBUF:
    DEFB    0, 0

ORG 13B0h
    LD HL,HEXCH
    LD (ESC),HL
    LD DE,MSG
    CALL DEPRT
;;; CLEAR
    LD HL,0FE80h
    JP 33BCh
MSG:
    DEFM "HEXCH V1.0"
    DEFB 0dh
    DEFM "ESC TO TRIGGER"
    DEFB 0
