;;; Type hex code directly to enter characters.
;;; ESC triggers the mode. Type 'A' 'A' in succession,
;;; for example, to input CHRS(&hAA). Type '00' to exit
;;; the mode. Non-hex code is ignored and gives a beep.
;;;
;;; Specify the entry point(13B0h) to auto-start
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

ORG 13B0h
    LD HL,0FF00h
    LD (ESC),HL
    LD DE,MSG
    CALL DEPRT
    RET
MSG:
    DEFM "HEXCH V1.0"
    DEFB 0dh
    DEFM "ESC TO TRIGGER"
    DEFB 0

ORG 0FF00h
    XOR A
    LD (CURTM),A
    LD DE,HEXBUF
HEXBUFL:
    CALL ASCGET
    LD (DE),A
    INC DE
    CALL ASCGET
    LD (DE),A
    DEC DE
    CALL HEX2IN
    JR C,HEXINPW
    OR A
    JR Z,HEXINPE
    CALL ACCPRT
    JR HEXBUFL
HEXINPW:
    CALL BELL
    JR HEXBUFL
HEXINPE:
    LD A,16
    LD (CURTM),A
    RET
HEXBUF:
    DEFB    0, 0
