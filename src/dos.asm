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
MOTCH:      EQU 0344H
SPPRT:      EQU 0862H
HEX2PR:     EQU 0562H
VBLOAD:     EQU 0259H
MOTON:      EQU 02E0H

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

;;; DOS command types
FGSAVE:     EQU 0D3H
FGLOAD:     EQU 0D4H
FGDIR:      EQU 0D5H
FGDEL:      EQU 0D6H

    incbin 'src/spcall.rom'

;;; Rewrite cassette IOCS to speed up read/write
;;; No more timing-based access but direct bit 0
;;; manipulation.
    seek 019DH
    org  019DH
CLOAD4:
    POP HL
    POP BC
    POP DE
    JP CREND

    ;; Just check Shift+BREAK
    seek 0230h
    org 0230h
EDGE:
MVRFY4:
    LD A,80H
    IN A,(0)
    AND 12H
    RET NZ
    SCF
    RET
    ;; Signals the read start
CRBEG:
    LD BC,6000H
    LD A,(IO6000)
    SET 5,A
    OUT (C),A
    LD HL,2828H
    RET
    ;; Mark the end of reading
CREND:
    PUSH BC
    LD BC,6000H
    LD A,(IO6000)
    RES 5,A
    OUT (C),A
    POP BC
    CALL MOTCH
    EI
    RET
    ;;

    seek 0289H
    org 0289H
    CALL DISCB1

    seek 0291H
    org 0291H
MKRD:
    PUSH BC
    PUSH DE
    PUSH HL
    CALL CRBEG

    ;; Do nothing in WAITR
    seek 03C4H
    org 03C4H
WAITR:
    RET

    seek 02D9H
    org 02D9H
    CALL DISCB1

    ;; Write 0 by clearing bit 0
    seek 03CBH
    org 03CBH
WRITES:
    PUSH AF
    PUSH BC
    LD BC,SMODE
    LD A,(IO6000)
    RES 0,A
    SET 7,A
    OUT (C),A
    RES 7,A
    LD (IO6000),A
    POP BC
    POP AF
    RET

    ;;  Write 1 by setting bit 0
    seek 03EDH
    org  03EDH
WRITEL:
    PUSH AF
    PUSH BC
    LD BC,SMODE
    LD A,(IO6000)
    SET 0,A
    SET 7,A
    OUT (C),A
    RES 7,A
    LD (IO6000),A
    POP BC
    POP AF
    RET
DISCB1:
    CALL EDGE
    CALL WAITR
    LD A,40H
    IN A,(1)
    RET

;;; Repurpose LET to DIR
    seek 2F98H
    org 2F98H
    DEFW DIR

;;; Define DEL entry point
    seek 2FCEH
    org 2FCEH
    DEFW DOSDEL

;;; Update ROPEN entry point
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
    CALL CR2

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
    LD HL,0C18H                 ; JR FSAVE1
    LD (009FH),HL
    CALL FSAVE
DOSREQ1:
    LD A,(IO6000)
    RES 4,A
    OUT (C),A
    LD (IO6000),A

    ;; Restore the code
    LD HL,11D5H
    LD (009FH),HL
    POP BC
    RET

    ;; Directory list
DIR:
    INC HL
    PUSH HL
    LD A,FGDIR
    CALL DOSREQ

    CALL FLOAD
    CALL MLOAD
    CALL MLEXEC
    CALL DOSEND
    POP HL
    RET

CLRFN:
    XOR A
    LD (FILNAM),A
    RET

;;; Read program name from the command param and set to FIB.
;;; Used by LOAD/SAVE <PROGRAM>
SETFN:
    CALL CLRFN
    CALL BCFTCH
    DEC BC
    OR A
    RET Z
    CP 03AH
    RET Z
    CALL STREXX
    DEC BC
    OR A
    RET Z
    PUSH BC
    CP 17
    JP NC,IFCALL
    LD B,0
    LD C,A
    EX DE,HL
    LD DE,FILNAM
    LDIR
    POP BC
    XOR A
    LD (DE),A
    RET

;;; Lets assembly programs run automatically after loading.
;;; Setting MTEXEC in FIB to the exec addr does the trick.
MLEXEC:
    LD HL,(MTEXEC)
    LD A,H
    OR L
    JR Z,ROPND
    LD DE,ROPND
    PUSH DE
    JP (HL)

ROPEN:
    LD C,L
    LD B,H
    CALL CROPEN
ROPND:
    LD L,C
    LD H,B
    RET

;;; Disable LOAD? command to repurpose the VERIFY code space
    seek 3939H
    org 3939H
    NOP

;;; Enable ASM program to autorun
    seek 39A7h
    org 39a7h
    JR C,TPRDER
    JP MLEXEC

;;; Unused msg LFOUM to 'NONAME'
    seek 3A43H
    org 3A43H
    DEFM 'NONAME'

;;; CWOPEN updated to use SETFN
    seek 3A5DH
    org 3A5DH
CWOPEN:
    LD A,(FILEFG)
    OR A
    JP NZ,FILEER
    LD A,2
    LD (FILEFG),A
    LD A,16H
    LD (FILMOD),A
    LD C,L
    LD B,H
    call SETFN
    DEFB 3Eh                    ; LD A,
DOSCMF:
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

DOSEND:
    XOR A
    JP DOSREQ

DOSDEL:
    XOR A
    LD (FILEFG),A
    LD A,FGDEL
    LD (DOSCMF),A
    CALL CWOPEN                 ; Use CWOPEN to run DEL
    LD H,B
    LD L,C
    LD A,FGSAVE
    LD (DOSCMF),A
    LD A,(FILNAM)
    OR A
    JP Z,SERROR
    RET

    ;; Stop button after SAVE
    seek 3B0EH
    org 3B0EH
CBRSHM:
    POP HL
    XOR A
    JP DOSEND

;;; Rename LET to DIR
    seek 679BH
    org 679BH
    DEFB 44H, 49H, 0D2H         ; DOS

;;; Add DEL cmd
;;; ERROR -> ERM
    seek 680CH
    org 680CH
    DEFB 44H, 45H, 0CCH         ; DEL
    DEFB 45H, 52H, 0CDH         ; ERM
