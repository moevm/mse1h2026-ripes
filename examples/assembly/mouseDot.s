# Set the addresses and sizes below from the I/O tab -> "Exports" panel:
#   LED_BASE   <- LED_MATRIX_0_BASE
#   LED_W      <- LED_MATRIX_0_WIDTH
#   LED_H      <- LED_MATRIX_0_HEIGHT
#   MOUSE_BASE <- MOUSE_0_BASE
#   MOUSE_W    <- MOUSE_0_WIDTH
#   MOUSE_H    <- MOUSE_0_HEIGHT
# Requires a processor with the M extension.

.equ LED_BASE,   0xF0000024
.equ LED_W,      25
.equ LED_H,      25

.equ MOUSE_BASE, 0xF0000010
.equ MOUSE_W,    500
.equ MOUSE_H,    500

.equ COLOR_RED,  0xFF0000

.text
.globl _start
_start:
    li   s0, LED_BASE
    li   s1, MOUSE_BASE

    li   t0, LED_W
    li   t1, LED_H
    mul  t0, t0, t1
    mv   t1, s0
clr:
    sw   zero, 0(t1)
    addi t1, t1, 4
    addi t0, t0, -1
    bnez t0, clr

main_loop:
    lw   a0, 0(s1)
    lw   a1, 4(s1)
    lw   a2, 8(s1)
    lw   a3, 12(s1)

    li   t0, LED_W
    mul  a0, a0, t0
    li   t0, MOUSE_W
    div  a0, a0, t0
    bgez a0, x_lo_ok
    li   a0, 0
x_lo_ok:
    li   t0, LED_W
    addi t0, t0, -1
    ble  a0, t0, x_hi_ok
    mv   a0, t0
x_hi_ok:

    li   t0, LED_H
    mul  a1, a1, t0
    li   t0, MOUSE_H
    div  a1, a1, t0
    bgez a1, y_lo_ok
    li   a1, 0
y_lo_ok:
    li   t0, LED_H
    addi t0, t0, -1
    ble  a1, t0, y_hi_ok
    mv   a1, t0
y_hi_ok:

    li   t0, LED_W
    mul  t1, a1, t0
    add  t1, t1, a0
    slli t1, t1, 2
    add  t1, s0, t1

    beqz a2, not_left
    li   t2, COLOR_RED
    sw   t2, 0(t1)
not_left:

    beqz a3, not_right
    sw   zero, 0(t1)
not_right:

    j    main_loop
