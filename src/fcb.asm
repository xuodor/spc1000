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

    ORG 13B0h

    LD HL,(MTADRS)
DOSPRT:
    LD A,(HL)
    INC HL
    OR A
    JR Z,DOSPNX
    CALL ACCPRT
    JR DOSPRT

DOSPNX:
    CALL TABPRT
    LD A,(HL)
    OR A
    JR NZ,DOSPRT
    ;; The number of files
    LD A,(FILNAM)
    CP 0FFh
    JR NZ,DOSPRTE
    LD DE,DOSPMG
    CALL DEPRT

DOSPRTE:
    RET
DOSPMG:
    DEFB 28h, 22h, 22h, 22h, 29h ; (...)
    DEFB 0
