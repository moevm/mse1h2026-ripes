# mouseDrawing.s
# Drawing on the LED matrix using the mouse peripheral.
#
# Controls:
#   * Hold the left mouse button to draw at the cursor position.
#     (Unlike the C version, this assembly version places single pixels at
#      each polled position rather than connecting them with line segments.)
#   * Hold the right mouse button to erase a small disc around the cursor.
#   * Move the scroll wheel to cycle through colors: red -> green -> blue.
#
# To run this program, instantiate the following peripherals in the I/O tab:
# - "LED Matrix"
# - "Mouse"

.data
colors:     .word 0x00FF0000, 0x0000FF00, 0x000000FF

.eqv ERASE_R,   5
.eqv ERASE_R2,  25      # ERASE_R * ERASE_R

.text
main:
    li   s0, LED_MATRIX_0_BASE          # LED matrix base
    li   s1, MOUSE_0_BASE               # mouse base
    li   s2, LED_MATRIX_0_WIDTH         # W
    li   s3, LED_MATRIX_0_HEIGHT        # H
    li   s4, MOUSE_0_WIDTH              # mouse range width
    li   s5, MOUSE_0_HEIGHT             # mouse range height
    li   s6, 0                          # color index (0..2)
    li   s7, 0                          # previous scroll value

    # Clear LED matrix
    mul  t0, s2, s3
    mv   t1, s0
clear_led:
    beqz t0, poll_loop
    sw   zero, 0(t1)
    addi t1, t1, 4
    addi t0, t0, -1
    j    clear_led

# --- Main polling loop ---
poll_loop:
    lw   a0, 0(s1)                      # mouse X
    lw   a1, 4(s1)                      # mouse Y
    lw   a2, 8(s1)                      # left button
    lw   a3, 12(s1)                     # right button
    lw   a4, 16(s1)                     # scroll

    # ledX = mouseX * W / MOUSE_W
    mul  t0, a0, s2
    divu t0, t0, s4
    addi t1, s2, -1
    bge  t1, t0, x_ok
    mv   t0, t1
x_ok:
    bgez t0, x_pos
    li   t0, 0
x_pos:
    mv   a5, t0                         # a5 = ledX

    # ledY = mouseY * H / MOUSE_H
    mul  t0, a1, s3
    divu t0, t0, s5
    addi t1, s3, -1
    bge  t1, t0, y_ok
    mv   t0, t1
y_ok:
    bgez t0, y_pos
    li   t0, 0
y_pos:
    mv   a6, t0                         # a6 = ledY

    # Update color when scroll changes
    beq  a4, s7, scroll_done
    bgt  a4, s7, scroll_up
    # scroll down: idx = (idx + 2) % 3
    addi s6, s6, 2
    j    scroll_norm
scroll_up:
    addi s6, s6, 1
scroll_norm:
    li   t0, 3
    rem  s6, s6, t0
    mv   s7, a4
scroll_done:

    # If left button pressed, draw a pixel at (a5, a6)
    beqz a2, check_right
    la   t0, colors
    slli t1, s6, 2
    add  t0, t0, t1
    lw   t2, 0(t0)                      # current color
    mul  t3, a6, s2
    add  t3, t3, a5
    slli t3, t3, 2
    add  t3, t3, s0
    sw   t2, 0(t3)

check_right:
    beqz a3, poll_loop
    # Erase disc of radius ERASE_R around (a5, a6)
    li   t0, -ERASE_R                   # dy
erase_dy:
    li   t1, ERASE_R
    bgt  t0, t1, poll_loop
    li   t2, -ERASE_R                   # dx
erase_dx:
    li   t3, ERASE_R
    bgt  t2, t3, erase_dx_end
    # if dx*dx + dy*dy > ERASE_R2 -> skip
    mul  t4, t2, t2
    mul  t5, t0, t0
    add  t4, t4, t5
    li   t6, ERASE_R2
    bgt  t4, t6, erase_skip
    # x = a5 + dx ; y = a6 + dy
    add  t4, a5, t2
    add  t5, a6, t0
    bltz t4, erase_skip
    bge  t4, s2, erase_skip
    bltz t5, erase_skip
    bge  t5, s3, erase_skip
    mul  t6, t5, s2
    add  t6, t6, t4
    slli t6, t6, 2
    add  t6, t6, s0
    sw   zero, 0(t6)
erase_skip:
    addi t2, t2, 1
    j    erase_dx
erase_dx_end:
    addi t0, t0, 1
    j    erase_dy
