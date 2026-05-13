# keyboardSevenSeg.s
# Reads characters from the keyboard peripheral and shows the most
# recent digit/letter on digit 0 of the 7-segment display.
#
# Required peripherals (in I/O tab): "Keyboard", "Seven Segment".
#
# !!! TODO: set the addresses below from the I/O tab -> "Exports" panel !!!
#   Copy SEVEN_SEGMENT_0_BASE into SEG_BASE.
#   Copy KEYBOARD_0_BASE      into KBD_BASE.
#   Defaults assume:
#     SEVEN_SEGMENT_0_BASE = 0xF0000000
#     KEYBOARD_0_BASE      = 0xF0000DD0 (+0 = char, +4 = available)

.equ SEG_BASE, 0xF0000000   # <-- set from Exports: SEVEN_SEGMENT_0_BASE
.equ KBD_BASE, 0xF0000DD0   # <-- set from Exports: KEYBOARD_0_BASE

# ASCII codes (Ripes assembler does not support 'x' character literals)
.equ ASCII_0, 0x30
.equ ASCII_9, 0x39
.equ ASCII_A, 0x41
.equ ASCII_Z, 0x5A
.equ ASCII_a, 0x61
.equ ASCII_z, 0x7A

.text
.globl _start
_start:
    li   s0, KBD_BASE
    li   s1, SEG_BASE
    la   s2, seg_digit
    la   s3, seg_alpha

loop:
    lw   t0, 4(s0)          # available?
    beqz t0, loop
    lw   t0, 0(s0)          # key code
    andi t0, t0, 0xFF

    # '0'..'9'
    li   t1, ASCII_0
    blt  t0, t1, no_digit
    li   t1, ASCII_9
    bgt  t0, t1, no_digit
    addi t0, t0, -ASCII_0
    add  t1, s2, t0
    lbu  t2, 0(t1)
    j    show

no_digit:
    # 'A'..'Z'
    li   t1, ASCII_A
    blt  t0, t1, no_alpha_u
    li   t1, ASCII_Z
    bgt  t0, t1, no_alpha_u
    addi t0, t0, -ASCII_A
    j    take_alpha
no_alpha_u:
    # 'a'..'z'
    li   t1, ASCII_a
    blt  t0, t1, loop
    li   t1, ASCII_z
    bgt  t0, t1, loop
    addi t0, t0, -ASCII_a
take_alpha:
    add  t1, s3, t0
    lbu  t2, 0(t1)

show:
    sw   t2, 0(s1)          # write to digit 0
    j    loop

.data
seg_digit:
    .byte 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
seg_alpha:
    .byte 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x3D, 0x76, 0x06, 0x1E
    .byte 0x76, 0x38, 0x37, 0x54, 0x3F, 0x73, 0x67, 0x50, 0x6D, 0x78
    .byte 0x3E, 0x1C, 0x3E, 0x76, 0x6E, 0x5B
