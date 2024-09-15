;;; Print out the file list in response to the command DOSV,
;;; 4 files per row.
;;; The code is placed inside the FCB extension area, with
;;; the file list in the body. This can save the amount of
;;; the code to be put in the interpreter.

ACCPRT: EQU 0864h
TABPRT: EQU 0802h
FILNAM: EQU 1397h
MTADRS: EQU 13AAh
DEPRT:  EQU 07F3h
SPPRT:  EQU 0862H

    ORG 13AEh
    NOP
    NOP
    ;; # of files
    LD A,(FILNAM)
    OR A
    RET Z

    ;; file list
    LD HL,(MTADRS)
    PUSH BC
FNAME:
    LD B,8
FNCHAR:
    LD A,(HL)
    INC HL
    OR A
    JR Z,SPCCH
    CALL ACCPRT
    DEC B
    JR FNCHAR

SPCCH:
    CALL SPPRT
    DJNZ SPCCH

    ;; Additional zero indicates the end of the list
    LD A,(HL)
    OR A
    JR NZ,FNAME

MOREFL:
    ;; More files?
    LD A,(FILNAM)
    CP 0FFh
    JR NZ,DIREND
    LD DE,MOREMSG
    CALL DEPRT
DIREND:
    POP BC
    RET

MOREMSG:
    DEFM '(...)'
    DEFB 0
