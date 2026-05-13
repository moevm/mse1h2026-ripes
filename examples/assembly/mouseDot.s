# mouseDot.s
# Simplified assembly version of examples/C/mouseDrawing.c.
# - Hold the left mouse button to paint a red pixel under the cursor.
# - Hold the right mouse button to erase the pixel under the cursor.
#
# Required peripherals (in I/O tab): "LED Matrix", "Mouse".
# Requires a processor with the M extension (mul/div).
#
# !!! TODO: set the addresses and sizes below from the I/O tab -> "Exports" panel !!!
#   Copy LED_MATRIX_0_BASE   -> LED_BASE
#   Copy LED_MATRIX_0_WIDTH  -> LED_W
#   Copy LED_MATRIX_0_HEIGHT -> LED_H
#   Copy MOUSE_0_BASE        -> MOUSE_BASE
#   Copy MOUSE_0_WIDTH       -> MOUSE_W
#   Copy MOUSE_0_HEIGHT      -> MOUSE_H

.equ LED_BASE,   0xF0000024   # <-- set from Exports: LED_MATRIX_0_BASE
.equ LED_W,      25           # <-- set from Exports: LED_MATRIX_0_WIDTH
.equ LED_H,      25           # <-- set from Exports: LED_MATRIX_0_HEIGHT

.equ MOUSE_BASE, 0xF0000010   # <-- set from Exports: MOUSE_0_BASE
.equ MOUSE_W,    500          # <-- set from Exports: MOUSE_0_WIDTH
.equ MOUSE_H,    500          # <-- set from Exports: MOUSE_0_HEIGHT

.equ COLOR_RED,  0xFF0000

.text
.globl _start
_start:
    li   s0, LED_BASE
    li   s1, MOUSE_BASE

    # clear the LED matrix
    li   t0, LED_W
    li   t1, LED_H
    mul  t0, t0, t1            # W*H
    mv   t1, s0
clr:
    sw   zero, 0(t1)
    addi t1, t1, 4
    addi t0, t0, -1
    bnez t0, clr

main_loop:
    lw   a0, 0(s1)             # mouseX
    lw   a1, 4(s1)             # mouseY
    lw   a2, 8(s1)             # left button
    lw   a3, 12(s1)            # right button

    # ledX = mouseX * LED_W / MOUSE_W, clamp [0, LED_W-1]
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

    # ledY = mouseY * LED_H / MOUSE_H, clamp [0, LED_H-1]
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

    # pixel address = led + (y*W + x)*4
    li   t0, LED_W
    mul  t1, a1, t0
    add  t1, t1, a0
    slli t1, t1, 2
    add  t1, s0, t1

    # left button -> draw red
    beqz a2, not_left
    li   t2, COLOR_RED
    sw   t2, 0(t1)
not_left:

    # right button -> erase
    beqz a3, not_right
    sw   zero, 0(t1)
not_right:

    j    main_loop
