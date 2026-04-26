# sevenSegKeyboard.s
# Reads characters from a keyboard peripheral and writes the corresponding
# 7-segment encoding into successive digits of a 7-segment display.
#
# To run this program, instantiate the following peripherals in the I/O tab:
# - "Keyboard"
# - "7-Segment Display"

.data
# 7-segment encodings for digits '0'..'9' (one byte per character)
seg_digit:  .byte 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F

# 7-segment encodings for letters 'A'..'Z'
seg_alpha:  .byte 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x3D, 0x76, 0x06, 0x1E
            .byte 0x76, 0x38, 0x37, 0x54, 0x3F, 0x73, 0x67, 0x50, 0x6D, 0x78
            .byte 0x3E, 0x1C, 0x3E, 0x76, 0x6E, 0x5B

.text
main:
    li   s0, SEVEN_SEGMENT_0_BASE       # base address of 7-segment device
    li   s1, KEYBOARD_0_BASE            # base address of keyboard device
    li   s2, SEVEN_SEGMENT_0_N_DIGITS   # total number of digits
    li   s3, 0                          # current digit position

    # Clear all 7-segment digits
    mv   t0, s0
    mv   t1, s2
clear_loop:
    beqz t1, wait_loop
    sw   zero, 0(t0)
    addi t0, t0, 4
    addi t1, t1, -1
    j    clear_loop

# Main polling loop: wait for a key, encode it, write to next digit
wait_loop:
    lw   t0, 4(s1)                      # keyboard "available" register
    beqz t0, wait_loop                  # no key -> keep polling

    lw   a0, 0(s1)                      # read key code (consumes it)
    andi a0, a0, 0xFF                   # keep low 8 bits

    jal  ra, encode                     # a0 -> 7-segment code in a0
    beqz a0, wait_loop                  # unsupported character -> ignore

    # Write code to current digit: address = s0 + s3 * 4
    slli t1, s3, 2
    add  t1, t1, s0
    sw   a0, 0(t1)

    # Advance position with wrap-around
    addi s3, s3, 1
    bne  s3, s2, wait_loop
    li   s3, 0
    j    wait_loop

# --- encode ---
# Convert ASCII char in a0 to 7-segment code in a0.
# Returns 0 if the character is not a digit or a letter.
encode:
    li   t0, '0'
    blt  a0, t0, enc_zero
    li   t0, '9'
    bgt  a0, t0, enc_try_upper

    # digit
    li   t0, '0'
    sub  t1, a0, t0
    la   t2, seg_digit
    add  t2, t2, t1
    lbu  a0, 0(t2)
    ret

enc_try_upper:
    li   t0, 'A'
    blt  a0, t0, enc_zero
    li   t0, 'Z'
    ble  a0, t0, enc_alpha

    # try lowercase
    li   t0, 'a'
    blt  a0, t0, enc_zero
    li   t0, 'z'
    bgt  a0, t0, enc_zero
    addi a0, a0, -32                    # to uppercase

enc_alpha:
    li   t0, 'A'
    sub  t1, a0, t0
    la   t2, seg_alpha
    add  t2, t2, t1
    lbu  a0, 0(t2)
    ret

enc_zero:
    li   a0, 0
    ret
