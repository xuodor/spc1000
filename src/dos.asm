;;; IOCS
FSAVE:      EQU 0080H
FLOAD:      EQU 0114H
MLOAD:      EQU 0134H
FILMOD:     EQU 1396H
FILNAM:     EQU 1397H
MTBYTE:     EQU 13A8H
MTADRS:     EQU 13AAH
MTEXEC:     EQU 13ACH
PROTCT:     EQU 13AEH
IO6000:     EQU 11E9H
DEPRT:      EQU 07F3H
TABPRT:     EQU 0802H
CR2:        EQU 0823H
PRTSDC:     EQU 07E1H

;;; BASIC
BREAK:      EQU 16A2H
IFCALL:     EQU 1736H
SERROR:     EQU 173AH
FILEER:     EQU 183EH
FILEFG:     EQU 3383H
SAVE:       EQU 3ABBH
CBREAK:     EQU 3A26H
CBRSHM1:    EQU 3B0FH
LOADST2:    EQU 389DH
LDFNTR2:    EQU 3892H
LOAD:       EQU 392CH
SMODE:      EQU 6000H
CWOPEN:     EQU 3A5DH
NULFNM:     EQU 3A98H
STREXX:     EQU 459DH
BCFTCH:     EQU 25C4H
SETLFN:     EQU 3A69H
SETLFNB:    EQU 3A9CH
ERRTXT:     EQU 69B2H
RTBLOD:     EQU 39A1h
TPRDER:     EQU 3969h
BUFCLR:     EQU 4511H
LDINGM:     EQU 3A49H
VSTART:     EQU 7A3BH
TEXTST:     EQU 7C9DH

;;; DOS
FGSAVE:     EQU 0D3H
FGLOAD:     EQU 0D4H
FGDIR:      EQU 0D5H
FGDEL:      EQU 0D6H

;;; DOSV RESPONSE
FILECNT:    EQU 1396H
FILEBUF:    EQU 1397H

    incbin 'src/spcall.rom'

;;; Repurpose LET to DIR
    seek 2F98H
    org 2F98H
    DEFW DIR

;;; Define DEL command entry
    seek 2FCEH
    org 2FCEH
    DEFW DOSDEL

;;; Adjust ROPEN entry point
    seek 3000H
    org 3000H
    DEFW ROPEN

;;; Rewrite CROPEN for new LOAD command processing
    seek 3865H
    org 3865H
CROPEN:
    LD A,(FILEFG)
    OR A
    JP NZ,FILEER
    INC A
    LD (FILEFG),A
    CALL SETFN

    LD A,FGLOAD
    CALL DOSREQ
    CALL FLOAD
    JP C,BREAK
    CALL CR2
LFNOK:
    PUSH BC
    CALL BUFCLR
    LD DE,LDINGM
    LD B,8
    CALL PRTSDC
    LD DE,FILNAM
    CALL DEPRT
    CALL CR2
    POP BC
    RET

DOSREQ:
    LD (FILMOD),A
    PUSH BC
    PUSH AF
    LD BC,SMODE
    LD A,(IO6000)
    SET 4,A
    OUT (C),A
    LD (IO6000),A
    POP AF
    OR A
    JR Z, DOSREQ1

    ;; Do not display WRITING: <FILNAME>
    LD HL,0C18H                 ; JR 00ACH
    LD (009FH),HL
    CALL FSAVE
DOSREQ1:
    LD A,(IO6000)
    RES 4,A
    OUT (C),A
    LD (IO6000),A

    ;; Restore the code
    LD HL,11D5H                 ; PUSH DE, LD DE,
    LD (009FH),HL
    POP BC
    RET

DOSDEL:
    XOR A
    LD (FILEFG),A
    LD A,FGDEL
    LD (DOSCMB),A
    CALL CWOPEN

    ;; Restore text pointer
    LD H,B
    LD L,C

    LD A,FGSAVE
    LD (DOSCMB),A
    RET

;;; Lets assembly programs run automatically after loading.
;;; Setting MTEXEC in FIB to the right start address.
MLEXEC:
    LD HL,(MTEXEC)
    LD A,H
    OR L
    JP Z,RTBLOD
    LD DE,RTBLOD
    PUSH DE
    JP (HL)

SETFN:
    CALL BCFTCH
    DEC BC
    LD DE,FILNAM
    OR A
    JR Z,NULFN
    CP 03AH
    JR Z,NULFN
    CALL STREXX

    DEC BC
    PUSH BC
    CP 17
EMPTST:
    JP NC,IFCALL
    LD B,0
    LD C,A
    EX DE,HL
    LD DE,FILNAM
    LDIR
    POP BC
NULFN:
    XOR A
    LD (DE),A
    RET

ROPEN:
    LD C,L
    LD B,H
    CALL CROPEN
    LD L,C
    LD H,B
    RET

;;; Disable LOAD? command to repurpose the code space
;;; for VERIFY: to DOSDEL
    seek 3939H
    org 3939H
    NOP

;;; Enable ASM program to autorun
    seek 39A7h
    org 39a7h
    JR C,TPRDER
    JP MLEXEC

;;; CWOPEN
    ;; Use SETFN to save space for DEL/autorun
    seek 3A6BH
    org 3A6BH
    call SETFN

    DEFB 3Eh                    ; LD A,
DOSCMB:
    DEFB FGSAVE
    CALL DOSREQ

    LD HL,TEXTST
    LD (MTADRS),HL
    EX DE,HL
    LD HL,(VSTART)
    OR A
    SBC HL,DE
    LD (MTBYTE),HL
    LD HL,0
    LD (MTEXEC),HL
    XOR A
    LD (PROTCT),A
    CALL BUFCLR
    RET

DOSSAV1:
    JP C,CBREAK
DOSEND:
    XOR A
    JP DOSREQ

    ;; Directory list
DIR:
    INC HL
    PUSH HL
    LD A,FGDIR
    CALL DOSREQ

    CALL FLOAD
    JP C,BREAK
    CALL MLOAD
    JP C,BREAK
    CALL MLEXEC
    CALL DOSEND
    POP HL
    RET

    ;; Stop button at the end of SAVE
    seek CBRSHM1
    org CBRSHM1
    CALL DOSSAV1

;;; Update the reference to UDLMES

    seek 3F6BH
    org 3f6BH
    LD DE,UDLMES2

;;; Rename LET to DIR
    seek 679BH
    org 679BH
    DEFB 44H, 49H, 0D2H                 ; DOS

;;; Add DEL cmd. ERROR -> ERR
    seek 680CH
    org 680CH
    DEFB 44H, 45H, 0CCH         ; DEL
    DEFB 45H, 52H, 0D2H         ; ERR

;;; Shortens the error message to give more code space to
;;; new commands.

    seek ERRTXT
    org ERRTXT

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
    DEFM "HB"
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
