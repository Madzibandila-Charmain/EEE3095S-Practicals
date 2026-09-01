/*
 * lcd.s
 * EEE3096S 2026 - Practical 1B, Task 5
 * 4-bit bit-banged HD44780 driver, and the level shifter timing fault
 *
 * Student 1 : Mosa Mdladla MNYMOS010
 * Student 2 : Charmain Madzibandila MDZMUT005
 */

    .syntax unified
    .thumb
    .cpu    cortex-m0
    .fpu    softvfp

    .global LCD_Run
    .type   LCD_Run, %function

@ ---------------------------------------------------------------------------
@ Register addresses. BSRR is at offset 0x18 from each port base.
@ ---------------------------------------------------------------------------
    .equ GPIOA_BSRR, 0x48000018
    .equ GPIOB_BSRR, 0x48000418
    .equ GPIOC_BSRR, 0x48000818

@ ---------------------------------------------------------------------------
@ PIN MAP
@   PC15  Enable (E)     -> PC15_S on the 5 V side
@   PC14  Register Select (RS)
@   PB8   D4      PB9   D5      PA12  D6      PA15  D7
@   R/W is tied to ground. The LCD is write only.
@ ---------------------------------------------------------------------------

    .section .text.LCD_Run, "ax", %progbits

@ ===========================================================================
@ ENTRY POINT
@ ===========================================================================
LCD_Run:
    PUSH {LR}

    @ TODO 1: Wait for the LCD power rail to settle (consult datasheet).
    LDR R0,=40
    BL LCD_DelayLong
    @ TODO 2: Call the 4-bit initialization sequence.
    BL LCD_Init
    @ TODO 3: Write the character 'A' (0x41) to the display.
    LDR R0, =0x41
    BL LCD_WriteData
hang:
   B    hang

    .size LCD_Run, .-LCD_Run

@ ===========================================================================
@ LCD_Init
@ Puts the controller into 4-bit mode and readies the display.
@ ===========================================================================
    .type LCD_Init, %function
LCD_Init:
    PUSH {LR}

    @ TODO 4: Send the 4-bit initialization sequence.
    @ Reference the HD44780 datasheet flowchart.
    @ Send commands with RS low using LCD_WriteCmd.

    MOVS R0, #0b0011
    BL LCD_SendNibble               @Takes 4-bit value and copies this in R0. LCD_SendNibble sends this so that it can distribute it across the four pins of PB8,PB9,PA12,PA15
    BL LCD_Pulse                    @Calls the LCD_Pulse which indicates to LCD when an enable pulse occurs so that it can also start reading the pins
    LDR R0, =5                      @delay parameter used to determine how long to wait
    BL LCD_DelayLong                @calls delay function to execute the delay length

    MOVS R0, #0b0011             @Same as above, and following sequence of datasheet
    BL LCD_SendNibble
    BL LCD_Pulse
    LDR R0, =150
    BL LCD_DelayShort


    MOVS R0, #0b0011
    BL   LCD_SendNibble
    BL   LCD_Pulse
    LDR  R0, =150            @ wait > 37 us
    BL   LCD_DelayShort

    MOVS R0, #0b0010
    BL   LCD_SendNibble
    BL   LCD_Pulse
    LDR  R0, =150
    BL   LCD_DelayShort

    @ --- Function set
    LDR  R0, =0b00101000
    BL   LCD_WriteCmd
    LDR  R0, =150
    BL   LCD_DelayShort

    LDR  R0, =0b00001000
    BL   LCD_WriteCmd
    LDR  R0, =150
    BL   LCD_DelayShort

    LDR  R0, =0b00000001
    BL   LCD_WriteCmd
    LDR  R0, =2              @ wait > 1.52 ms
    BL   LCD_DelayLong

    LDR  R0, =0b00000110
    BL   LCD_WriteCmd
    LDR  R0, =150
    BL   LCD_DelayShort

    LDR  R0, =0b00001100
    BL   LCD_WriteCmd
    LDR  R0, =150
    BL   LCD_DelayShort

    POP  {PC}

@ ===========================================================================
@ LCD_WriteCmd   R0 = command byte, RS low
@ LCD_WriteData  R0 = data byte,    RS high
@ Both send the high nibble first, then the low nibble.
@ ===========================================================================
    .type LCD_WriteCmd, %function
LCD_WriteCmd:
    PUSH {R0, LR}
    @ TODO 5: Drive RS (PC14) LOW, then fall through to the shared sender.
    LDR R1, =GPIOC_BSRR
    LDR R2, =(0x01<<30)
    STR R2, [R1]
    B LCD_Send8

    .type LCD_WriteData, %function
LCD_WriteData:
    PUSH {R0, LR}
    @ TODO 6: Drive RS (PC14) HIGH, then fall through.
    LDR R1, =GPIOC_BSRR
    LDR R2, =(0x01<<14)
    STR R2, [R1]

LCD_Send8:
    @ TODO 7: Send the upper nibble of R0, pulse Enable,
    @         then the lower nibble of R0, pulse Enable again.
    MOVS R3, R0
    LSRS R0, R0, #4          @Shifts the value right to obtain the higher nibble
    BL LCD_SendNibble
    BL LCD_Pulse

    MOVS R0, R3              @Obtains lower nibble
    MOVS R1, #0x0F
    ANDS R0, R0, R1
    BL LCD_SendNibble
    BL LCD_Pulse

    POP {R0, PC}


@ ===========================================================================
@ LCD_SendNibble   R0 bits 3:0 -> the four data lines
@ ===========================================================================
    .type LCD_SendNibble, %function
LCD_SendNibble:
    PUSH {R1, R2, R3, LR}

    @ --- D4 (bit0) -> PB8 ---
    LDR  R1, =GPIOB_BSRR        @loading address of Bit Set/Reset Register
    LSLS R2, R0, #31            @Shifting value of R0 by 31 to determine if it is a 1 or a 0
    BPL  IF_PB8_LOW             @if the value is high, continues in this branch, if not, moves to the IF_PB8_LOW branch and resets PB8 as low (bit 24)
    MOVS R3, #1
    LSLS R3, R3, #8
    STR  R3, [R1]
    B    PB8_CONTINUE
IF_PB8_LOW:
    MOVS R3, #1
    LSLS R3, R3, #(8+16)
    STR  R3, [R1]
PB8_CONTINUE:
    @ --- D5 (bit1) -> PB9 ---
    LSLS R2, R0, #30
    BPL  IF_PB9_LOW
    MOVS R3, #1
    LSLS R3, R3, #9
    STR  R3, [R1]
    B    PB9_CONTINUE
IF_PB9_LOW:
    MOVS R3, #1
    LSLS R3, R3, #(9+16)
    STR  R3, [R1]

PB9_CONTINUE:
    @ --- D6 (bit2) -> PA12 ---
    LDR  R1, =GPIOA_BSRR
    LSLS R2, R0, #29
    BPL  IF_PA12_LOW
    MOVS R3, #1
    LSLS R3, R3, #12
    STR  R3, [R1]
    B    PA12_CONTINUE
IF_PA12_LOW:
    MOVS R3, #1
    LSLS R3, R3, #(12+16)
    STR  R3, [R1]

PA12_CONTINUE:
    @ --- D7 (bit3) -> PA15 ---
    LSLS R2, R0, #28
    BPL  IF_PA15_LOW
    MOVS R3, #1
    LSLS R3, R3, #15
    STR  R3, [R1]
    B    PA15_CONTINUE
IF_PA15_LOW:
    MOVS R3, #1
    LSLS R3, R3, #(15+16)
    STR  R3, [R1]

PA15_CONTINUE:
    POP  {R1, R2, R3, PC}

@ ===========================================================================
@ LCD_Pulse
@ ===========================================================================
    .type LCD_Pulse, %function
LCD_Pulse:
    PUSH {R0, R1, R2, LR}

    LDR  R0, =GPIOC_BSRR

    @ TODO 9: Set PC15 HIGH.
    MOVS R1, #1
    LSLS R1, R1, #15
    STR R1, [R0]

    @ -----------------------------------------------------------------
    @ TODO 10: THE TIMING FIX
    @ Implement a calculated pad delay here to overcome the RC time
    @ constant of the level shifter and meet the HD44780 hold time requirements.
    @ Show your cycle arithmetic in the comments.
    @ -----------------------------------------------------------------

    @ TODO 11: Set PC15 LOW.
    MOVS R1, #1
    LSLS R1, R1, #(15+16)
    STR R1, [R0]

    @ TODO 12: Hold Enable low long enough to meet the LCD cycle time.
    MOVS R0, #2
    BL LCD_DelayShort

    POP {R0, R1, R2, PC}

@ ===========================================================================
@ Delay helpers
@ ===========================================================================
    .type LCD_DelayLong, %function
LCD_DelayLong:
    @ SYSCLK = 8 MHz = 8,000 cycles = 1 ms
    @ loop = SUBS(1 cycle) + BNE taken(2 cycles) = 3 cycles PER iteration
    @ iterations per ms = 8000 / 3 = 2666.67 = 2667
    PUSH {R1, LR}
    LDR  R1, =2667
    //SUBS R1, R1, #1
    MULS R1, R1, R0
long_inner_loop:
    SUBS R1, R1, #1
    BNE  long_inner_loop
    POP  {R1, PC}

    .type LCD_DelayShort, %function
LCD_DelayShort:
    @ R0 = number of microseconds to wait.
    @ SYSCLK = 8 MHz -> 8 cycles = 1 us
    @ loop body = 3 cycles/iteration -> iterations per us = 8/3 = 2.67
    PUSH {R1, LR}
    LDR  R1, =3
    MULS R1, R1, R0
short_inner_loop:
    SUBS R1, R1, #1
    BNE  short_inner_loop
    POP  {R1, PC}
