# Set the address and size below from the I/O tab -> "Exports" panel:
#   SEG_BASE <- SEVEN_SEGMENT_0_BASE
#   N_DIGITS <- SEVEN_SEGMENT_0_N_DIGITS

.equ SEG_BASE,    0xF0000000
.equ N_DIGITS,    4
.equ DELAY_LOOPS, 20

.equ SEG_6, 0x7D
.equ SEG_7, 0x07

.text
.globl _start
_start:
    li   s0, SEG_BASE
    li   s1, N_DIGITS
    li   s2, 0

main_loop:
    mv   t1, s0
    li   t2, 0
clr:
    sw   zero, 0(t1)
    addi t1, t1, 4
    addi t2, t2, 1
    blt  t2, s1, clr

    slli t0, s2, 2
    add  t0, s0, t0
    li   t1, SEG_6
    sw   t1, 0(t0)

    addi t2, s2, 1
    bge  t2, s1, no7
    slli t0, t2, 2
    add  t0, s0, t0
    li   t1, SEG_7
    sw   t1, 0(t0)
no7:

    li   t0, DELAY_LOOPS
delay:
    addi t0, t0, -1
    bnez t0, delay

    addi s2, s2, 1
    blt  s2, s1, main_loop
    li   s2, 0
    j    main_loop
