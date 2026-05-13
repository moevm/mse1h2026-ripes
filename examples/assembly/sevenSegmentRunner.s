# sevenSegmentRunner.s
# Assembly version of examples/C/sevenSegmentRunner.c
# Slides "67" across the 7-segment display.
#
# Required peripherals (in I/O tab): "Seven Segment".
#
# !!! TODO: set the address below from the I/O tab -> "Exports" panel !!!
#   Look for SEVEN_SEGMENT_0_BASE and paste it into SEG_BASE.
#   The default value below assumes SEVEN_SEGMENT_0_BASE = 0xF0000000.

.equ SEG_BASE,    0xF0000000   # <-- set from Exports: SEVEN_SEGMENT_0_BASE
.equ N_DIGITS,    4            # <-- set from Exports: SEVEN_SEGMENT_0_N_DIGITS
.equ DELAY_LOOPS, 20

.equ SEG_6, 0x7D
.equ SEG_7, 0x07

.text
.globl _start
_start:
    li   s0, SEG_BASE       # base address of 7-segment display
    li   s1, N_DIGITS       # number of digits
    li   s2, 0              # pos = 0

main_loop:
    # clear all digits
    mv   t1, s0
    li   t2, 0
clr:
    sw   zero, 0(t1)
    addi t1, t1, 4
    addi t2, t2, 1
    blt  t2, s1, clr

    # seg[pos] = SEG_6
    slli t0, s2, 2
    add  t0, s0, t0
    li   t1, SEG_6
    sw   t1, 0(t0)

    # if (pos+1 < N) seg[pos+1] = SEG_7
    addi t2, s2, 1
    bge  t2, s1, no7
    slli t0, t2, 2
    add  t0, s0, t0
    li   t1, SEG_7
    sw   t1, 0(t0)
no7:

    # delay loop
    li   t0, DELAY_LOOPS
delay:
    addi t0, t0, -1
    bnez t0, delay

    # pos = (pos + 1) % N
    addi s2, s2, 1
    blt  s2, s1, main_loop
    li   s2, 0
    j    main_loop
