; delay.asm - Cycle-accurate delay function for PRU
; Each iteration: SUB (1 cycle) + QBNE (1 cycle) = 2 cycles per iteration

    .global delay_cycles

delay_cycles:
    ; R14 contains the cycle count parameter
delay_loop:
    SUB  R14, R14, 1
    QBNE delay_loop, R14, 0
    JMP  R3.w2              ; Return (R3.w2 contains return address)
