;-----------------------------------------------------------------------------
;
;   Copyright (C) 2007-2011, Freescale Semiconductor, Inc. All Rights Reserved.
;   THIS SOURCE CODE, AND ITS USE AND DISTRIBUTION, IS SUBJECT TO THE TERMS
;   AND CONDITIONS OF THE APPLICABLE LICENSE AGREEMENT
;
;-----------------------------------------------------------------------------
;
;  Module: stall.s
;
;-----------------------------------------------------------------------------
    INCLUDE kxarm.h
    INCLUDE armmacros.s

    TEXTAREA

;------------------------------------------------------------------------------
;
;  Function: PMOALStall
;
;       This function provides a timed stall at the lowest level;
;       Provides the same functionality as OALStall
;
;  Parameters:
;       4 bytes integer (R0 in assembly or a single parameter as a C function)
;
;  Returns:
;       None
;
;  NOTES:
;      Hard coded based on 1 gigahertz speed core of iMX6Q;
;      Parameter is in milliseconds;
;      Tested with 60,000 the time stalled was 60.22 seconds;
;      The error is less than 0.5%;
;      The parameter range is [0, 524287] (up to about 8.7 minutes)
;
;------------------------------------------------------------------------------
    ALIGN 4
    LEAF_ENTRY PMOALStall

    MOV R0, R0, LSL #13 ; Multiply by 8,192
    ORR R0, R0, #1 ; Set the lowest bit in case the parameter is zero

LoopCountDown
    SUBS R0, R0, #1

    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP ; 10
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP ; 20
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP ; 30
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP ; 40
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP ; 50
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP ; 60
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP ; 70
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP ; 80
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP ; 90
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP ; 96

    BGT LoopCountDown

    RETURN

    LEAF_END PMOALStall

    END
