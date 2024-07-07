;;; IOCS
FSAVE:      EQU 0080H
FLOAD:      EQU 0114H
FILEMOD:    EQU 1396H
FILENAM:    EQU 1397H
IO6000:     EQU 11E9H
DEPRT:      EQU 07F3H
TABPRT:     EQU 0802H

;;; BASIC
IFCALL:     EQU 1736H
SERROR:     EQU 173AH
FILEER:     EQU 183EH
SAVE:       EQU 3ABBH
LOAD:       EQU 392CH
SMODE:      EQU 6000H
CWOPEN:     EQU 3A5DH
NULFNM:     EQU 3A98H
SETLFN:     EQU 3A69H
SETLFNB:    EQU 3A9CH
ERRTXT:     EQU 69B2H

;;; DOS
DOSSFG:     EQU 0D3H
DOSLFG:     EQU 0D4H
DOSVFG:     EQU 0D5H
DOSDFG:     EQU 0D6H

;;; DOS REQUEST
DOSCMDF:    EQU 1396H

;;; DOSV RESPONSE
FILECNT:    EQU 1396H
FILEBUF:    EQU 1397H

ORG ERRTXT

    DEFB 00H                    ;1
    DEFM "E:NEXT"
    DEFB 00H                    ;2
    DEFM "E:SYNTAX"
    DEFB 00H                    ;3
    DEFM "E:RETURN"
    DEFB 00H                    ;4
    DEFM "E:NO DATA"
    DEFB 00H                    ;5
    DEFM "E:BAD FNCALL"
    DEFB 00H                    ;6
    DEFM "E:OVERFLOW"
    DEFB 00H                    ;7
    DEFM "E:OOM"
    DEFB 00H
UDLMES2:                        ;8
    DEFM "E:UNDEF LN#"
    DEFB 00H                    ;9
    DEFB "E:INDEX"
    DEFB 00H                    ;10
    DEFB "E:DUPDEF"
    DEFB 00H                    ;11
    DEFM "E:DIV/0"
    DEFB 00H                    ;12
    DEFM "E:BAD DIRECT"
    DEFB 00H                    ;13
    DEFM "E:TYPE MISMATCH"
    DEFB 00H                    ;14
    DEFM "HUBASIC"
    DEFB 00H                    ;15
    DEFM "E:STR 2LONG"
    DEFB 00H                    ;16
    DEFM "E:CMPL EXPR"
    DEFB 00H                    ;17
    DEFM "E:NO CONTINUE"
    DEFB 00H                    ;18
    DEFM "E:BAD USRFN"
    DEFB 00H                    ;19
    DEFM "E:NO RESUME"
    DEFB 00H                    ;20
    DEFM "E:RESUME"
    DEFB 00H                    ;21
    DEFM "E:FORMAT"
    DEFB 00H                    ;22
    DEFM "E:MISSING OP"
    DEFB 00H                    ;23
    DEFM "E:REPEAT"
    DEFB 00H                    ;24
    DEFM "E:UNTIL"
    DEFB 00H                    ;25
    DEFM "E:WHILE"
    DEFB 00H                    ;26
    DEFM "E:WEND"
    DEFB 00H                    ;27
    DEFM "E:TAPE READ"
    DEFB 00H                    ;28
    DEFM "E:UNDEF DEV"
    DEFB 00H                    ;29
    DEFM "E:STMT N/A"
    DEFB 00H                    ;30
    DEFM "E:EMPTY STACK"
    DEFB 00H                    ;31
    DEFM "E:FILE"
    DEFB 00H

DOS:
    LD A,(HL)
    INC HL
    CP 4CH                       ;L
    JR Z,DOSLOAD
    CP 53H                       ;S
    JR Z,DOSSAVE
    CP 44h                       ;D
    JR Z,DOSDEL
    CP 56H                       ;V
    JP NZ,SERROR

    ;; View files
DOSVIEW:
    LD A,DOSVFG
    CALL DOSREQ
    CALL FLOAD
    LD A,(FILECNT)
    OR A
    JR Z,DOSEND
    LD B,A
    LD DE,FILEBUF
    PUSH HL

    ;; Output 4 files in a row
DOSPRT:
    CALL DEPRT
    CALL TABPRT

    ;; DE += 8
    LD A,8
    ADD A,E
    LD E,A
    ADC A,D
    SUB E
    LD D,A

    DJNZ DOSPRT

    ;; Clear FIB
    LD HL,FILEMOD
    XOR A
    LD B,80h
DOSPRT1:
    LD (HL),A
    INC HL
    DJNZ DOSPRT1
    POP HL

    ;; Send an empty command to finish
DOSEND:
    XOR A
    JR DOSREQ

    ;; Load files
DOSLOAD:
    CALL DOSFLST

    ;; Restore buf pointer
    LD H,B
    LD L,C

    LD A,DOSLFG
    CALL DOSREQ
    CALL LOAD
    JR DOSEND

    ;; Save files
DOSSAVE:
    ;; If SAVEM, adjust the buf before getting filename
    PUSH HL
    LD A,(HL)
    CP 4Dh                      ; M
    JR NZ,DOSSV1
    INC HL
DOSSV1:
    CALL DOSFLST
    POP HL
    LD A,DOSSFG
    CALL DOSREQ
    CALL SAVE
    JR DOSEND

    ;; Delete files
DOSDEL:
    CALL DOSFLST

    ;; Restore buf pointer
    LD H,B
    LD L,C

    LD A,DOSDFG
    CALL DOSREQ
    RET

    ;; Set filename to FIB
DOSFLST:
    LD A,0C9H                   ; RET
    LD (SETLFNB),A
    CALL SETLFN
    LD A,21H
    LD (SETLFNB),A

    ;; Empty filename not allowed
    LD A,(FILENAM)
    OR A
    JP Z,FILEER
    RET

    ;; Send DOS command to cassette
DOSREQ:
    LD (DOSCMDF),A
    PUSH AF
    LD BC,SMODE
    LD A,(IO6000)
    SET 4,A
    OUT (C),A
    LD (IO6000),A
    POP AF
    OR A
    JR Z, DOSREQ1

    ;; Do not display WRITING: <FILENAME>
    LD DE,009Fh
    LD A,18h
    LD (DE),A
    LD A,0Ch
    INC DE
    LD (DE),A

    CALL FSAVE

DOSREQ1:
    LD A,(IO6000)
    RES 4,A
    OUT (C),A
    LD (IO6000),A

    ;; Restore the code
    LD DE,009Fh
    LD A,0D5h
    LD (DE),A
    LD A,11h
    INC DE
    LD (DE),A
    RET

;;; Repurpose LET to DOS

ORG 1B1CH
    JP DOS

ORG 679BH
    DEFB    44H, 4FH, 0D3H                 ; DOS

;;; Update the reference to UDLMES

ORG 3F6BH
    LD DE,UDLMES2
