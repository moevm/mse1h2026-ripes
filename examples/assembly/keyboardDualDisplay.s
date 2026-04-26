# keyboardDualDisplay.s
# Read characters from a keyboard peripheral and show them on two devices:
#   * the LED matrix (5x7 bitmap font, drawn centered)
#   * the 7-segment display (encoded character, written to successive digits)
#
# To run this program, instantiate the following peripherals in the I/O tab:
# - "Keyboard"
# - "7-Segment Display"
# - "LED Matrix"   (any size >= 5x7)

.data
# --- 5x7 LED-matrix bitmap font for '0'..'9', 'a'..'z' ---
# 7 bytes per glyph, low 5 bits used (MSB = leftmost pixel).
font:
    # 0-9
    .byte 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E
    .byte 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E
    .byte 0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F
    .byte 0x1F, 0x01, 0x02, 0x06, 0x01, 0x11, 0x0E
    .byte 0x11, 0x11, 0x11, 0x1F, 0x01, 0x01, 0x01
    .byte 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E
    .byte 0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E
    .byte 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10
    .byte 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E
    .byte 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C
    # a-z
    .byte 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x00
    .byte 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E
    .byte 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E
    .byte 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E
    .byte 0x1F, 0x10, 0x1E, 0x10, 0x10, 0x10, 0x1F
    .byte 0x1F, 0x10, 0x1E, 0x10, 0x10, 0x10, 0x10
    .byte 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F
    .byte 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11
    .byte 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E
    .byte 0x01, 0x01, 0x01, 0x01, 0x01, 0x11, 0x0E
    .byte 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11
    .byte 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F
    .byte 0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11
    .byte 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11
    .byte 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E
    .byte 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10
    .byte 0x0E, 0x11, 0x11, 0x11, 0x11, 0x0E, 0x01
    .byte 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11
    .byte 0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E
    .byte 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04
    .byte 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E
    .byte 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04
    .byte 0x11, 0x11, 0x15, 0x15, 0x15, 0x15, 0x0A
    .byte 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11
    .byte 0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04
    .byte 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F

# 7-segment encodings for '0'..'9'
seg_digit:  .byte 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F

# 7-segment encodings for 'A'..'Z'
seg_alpha:  .byte 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x3D, 0x76, 0x06, 0x1E
            .byte 0x76, 0x38, 0x37, 0x54, 0x3F, 0x73, 0x67, 0x50, 0x6D, 0x78
            .byte 0x3E, 0x1C, 0x3E, 0x76, 0x6E, 0x5B

# Constants
.eqv CHAR_W,    5
.eqv CHAR_H,    7
.eqv COLOR_ON,  0x00FF00
.eqv COLOR_OFF, 0x000000

.text
main:
    li   s0, LED_MATRIX_0_BASE          # LED matrix base
    li   s1, SEVEN_SEGMENT_0_BASE       # 7-segment base
    li   s2, KEYBOARD_0_BASE            # keyboard base
    li   s3, LED_MATRIX_0_WIDTH         # matrix width
    li   s4, LED_MATRIX_0_HEIGHT        # matrix height
    li   s5, SEVEN_SEGMENT_0_N_DIGITS   # number of 7-seg digits

    # center coordinates: cx = (W-5)/2, cy = (H-7)/2
    addi t0, s3, -CHAR_W
    srai s6, t0, 1                      # s6 = center_x
    addi t0, s4, -CHAR_H
    srai s7, t0, 1                      # s7 = center_y

    li   s8, 0                          # current 7-seg position

    # Clear LED matrix
    mul  t0, s3, s4                     # total pixels
    mv   t1, s0
clear_led:
    beqz t0, clear_seg
    sw   zero, 0(t1)
    addi t1, t1, 4
    addi t0, t0, -1
    j    clear_led

clear_seg:
    mv   t0, s5
    mv   t1, s1
clear_seg_loop:
    beqz t0, poll_loop
    sw   zero, 0(t1)
    addi t1, t1, 4
    addi t0, t0, -1
    j    clear_seg_loop

# --- Main loop: wait for keyboard input ---
poll_loop:
    lw   t0, 4(s2)                      # keyboard available?
    beqz t0, poll_loop
    lw   a0, 0(s2)                      # consume key
    andi a0, a0, 0xFF

    # Compute font index in a1 (0..35), or -1 if unsupported
    mv   a1, a0
    li   t0, '0'
    blt  a1, t0, idx_bad
    li   t0, '9'
    bgt  a1, t0, idx_try_upper
    li   t0, '0'
    sub  a1, a1, t0
    j    have_index
idx_try_upper:
    li   t0, 'A'
    blt  a1, t0, idx_bad
    li   t0, 'Z'
    ble  a1, t0, idx_upper
    li   t0, 'a'
    blt  a1, t0, idx_bad
    li   t0, 'z'
    bgt  a1, t0, idx_bad
    li   t0, 'a'
    sub  a1, a1, t0
    addi a1, a1, 10
    j    have_index
idx_upper:
    li   t0, 'A'
    sub  a1, a1, t0
    addi a1, a1, 10

have_index:
    # Compute 7-seg code for the same character
    li   t0, '0'
    blt  a0, t0, poll_loop
    li   t0, '9'
    bgt  a0, t0, sc_try_upper
    la   t2, seg_digit
    li   t0, '0'
    sub  t1, a0, t0
    add  t2, t2, t1
    lbu  a2, 0(t2)
    j    have_seg
sc_try_upper:
    li   t0, 'A'
    blt  a0, t0, poll_loop
    li   t0, 'Z'
    ble  a0, t0, sc_alpha_upper
    li   t0, 'a'
    blt  a0, t0, poll_loop
    li   t0, 'z'
    bgt  a0, t0, poll_loop
    addi a0, a0, -32
sc_alpha_upper:
    la   t2, seg_alpha
    li   t0, 'A'
    sub  t1, a0, t0
    add  t2, t2, t1
    lbu  a2, 0(t2)

have_seg:
    beqz a2, poll_loop                  # ignore characters with no 7-seg code

    # Clear the 5x7 character area on the LED matrix
    mv   a3, s6                         # x = center_x
    mv   a4, s7                         # y = center_y
    li   a5, COLOR_OFF
    jal  ra, draw_blank

    # Draw character glyph
    mv   a3, s6
    mv   a4, s7
    li   a5, COLOR_ON
    jal  ra, draw_glyph

    # Write 7-segment code to current digit
    slli t0, s8, 2
    add  t0, t0, s1
    sw   a2, 0(t0)
    addi s8, s8, 1
    bne  s8, s5, poll_loop
    li   s8, 0
    j    poll_loop

# --- draw_blank ---
# Fill a CHAR_W x CHAR_H area at (a3, a4) on the LED matrix with color a5.
draw_blank:
    li   t3, CHAR_H                     # row counter
    mv   t4, a4                         # current y
db_row:
    beqz t3, db_done
    li   t5, CHAR_W                     # column counter
    mv   t6, a3                         # current x
db_col:
    beqz t5, db_next_row
    # offset = (t4 * width + t6) * 4
    mul  t0, t4, s3
    add  t0, t0, t6
    slli t0, t0, 2
    add  t0, t0, s0
    sw   a5, 0(t0)
    addi t6, t6, 1
    addi t5, t5, -1
    j    db_col
db_next_row:
    addi t4, t4, 1
    addi t3, t3, -1
    j    db_row
db_done:
    ret

# --- draw_glyph ---
# Render glyph #a1 at (a3, a4) on the LED matrix using color a5.
# Bits set in the glyph row are drawn; bits cleared are left untouched.
draw_glyph:
    addi sp, sp, -16
    sw   ra, 12(sp)
    sw   s9, 8(sp)
    sw   s10, 4(sp)
    sw   s11, 0(sp)

    # glyph_addr = font + a1 * 7
    la   s9, font
    slli t0, a1, 3
    sub  t0, t0, a1                     # a1 * 7
    add  s9, s9, t0

    li   s10, 0                         # row counter (0..6)
dg_row:
    li   t0, CHAR_H
    beq  s10, t0, dg_done
    add  t1, s9, s10
    lbu  s11, 0(t1)                     # row bitmap
    li   t2, 0                          # column 0..4 (left to right)
dg_col:
    li   t0, CHAR_W
    beq  t2, t0, dg_next_row
    # bit index from left: CHAR_W-1-t2
    li   t0, CHAR_W-1
    sub  t3, t0, t2
    srl  t4, s11, t3
    andi t4, t4, 1
    beqz t4, dg_skip
    # x = a3 + t2 ; y = a4 + s10
    add  t5, a3, t2
    add  t6, a4, s10
    mul  t0, t6, s3
    add  t0, t0, t5
    slli t0, t0, 2
    add  t0, t0, s0
    sw   a5, 0(t0)
dg_skip:
    addi t2, t2, 1
    j    dg_col
dg_next_row:
    addi s10, s10, 1
    j    dg_row

dg_done:
    lw   ra, 12(sp)
    lw   s9, 8(sp)
    lw   s10, 4(sp)
    lw   s11, 0(sp)
    addi sp, sp, 16
    ret
