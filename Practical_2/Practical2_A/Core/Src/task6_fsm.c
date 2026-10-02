/**
  ******************************************************************************
  * @file    task6_fsm.c
  * @brief   TASK 6 : NON-BLOCKING EEPROM TRANSACTION STATE MACHINE
  *
  * Restructure the Task 4 transaction so the main loop never stops:
  *
  *   request -> write-enable -> write -> wait for EEPROM
  *           -> read-back -> verify -> result
  *
  * Rules (handout, Task 6):
  *   - No HAL_Delay(), and no software busy-wait for the EEPROM's internal
  *     write cycle. You may use HAL_GetTick() to decide when the next status
  *     check is due.
  *   - Each call does a small amount of work, updates the state, and returns.
  *   - Do not put the whole transaction inside one blocking function called
  *     from the loop.
  *   - While a transaction is in progress the loop must still respond to PA3.
  *
  * Controls: PA0 starts, PA3 aborts. PB0..PB7 show the last byte read, PB11
  * (green) a successful verification, PB10 (red) a failed one.
  ******************************************************************************
  */

#include "prac2a.h"

/* TODO 6.1  Design your states. You need enough of them to tell apart at
 *           least: idle, write preparation, write transaction, EEPROM busy /
 *           status checking, read transaction, verification, and success or
 *           failure.
 *
 *           Draw the diagram first. It must show the initial state, the
 *           condition on every transition, and the success, failure and
 *           abort paths.
 *
 *           typedef enum { ... } ee_state_t;
 */

typedef enum
{
    EE_IDLE = 0,
    EE_WRITE_PREPARE,
    EE_WRITE,
    EE_WAIT_READY,
    EE_READ,
    EE_VERIFY,
    EE_SUCCESS,
    EE_FAILURE,
    EE_ABORT        /* 8  PA3: clear write latch, status LEDs off, then idle     */
} ee_state_t;

volatile uint8_t ee_state = EE_IDLE;
volatile uint8_t ee_last_read   = 0u;
volatile uint8_t ee_use_fsm     = 1u;

void update_eeprom_state_machine(uint32_t now)
{
    /* TODO 6.2  One step of your state machine.
     *
     *   - btn_start_edge (PA0) starts a transaction from idle.
     *   - btn_abort_edge (PA3) abandons the transaction in progress and returns
     *     to idle. Leave the SPI bus in a state the next transaction can use.
     *   - While the EEPROM is busy writing, do NOT wait in here. Work out when
     *     the next status check is due, remember it, and return.
     *   - Decide that the write has finished from the status register, never
     *     from elapsed time alone. Also decide what happens if the EEPROM never
     *     reports ready.
     *   - Store the byte read back in ee_last_read.
     *   - Clear each button edge once you have acted on it. */

        static uint32_t t_write_done = 0u;   /* tick when the WRITE transaction ended  */
    static uint32_t t_last_poll  = 0u;   /* tick of the last status-register check */
    uint8_t status;

    /* PA3: abandon whatever is in progress. Ignored when already idle. */
    if (btn_abort_edge)
    {
        btn_abort_edge = 0u;
        if (ee_state != EE_IDLE)
        {
            btn_start_edge = 0u;
            ee_state = EE_ABORT;
            return;                       /* update_outputs() shows the abort first */
        }
    }

    switch (ee_state)
    {
    case EE_IDLE:
    case EE_SUCCESS:
    case EE_FAILURE:
        if (btn_start_edge)
        {
            btn_start_edge = 0u;
            ee_state = EE_WRITE_PREPARE;
        }
        break;

    case EE_WRITE_PREPARE:
        if (eeprom_test_addr >= EEPROM_SIZE_BYTES)
        {
            eeprom_verify_ok = 0u;
            ee_state = EE_FAILURE;
            break;
        }
        eeprom_write_enable();
        status = eeprom_read_status();
        if (status & EEPROM_SR_WEL)
        {
            ee_state = EE_WRITE;
        }
        else
        {
            eeprom_verify_ok = 0u;        /* latch not set: a write would be ignored */
            ee_state = EE_FAILURE;
        }
        break;

    case EE_WRITE:
        eeprom_cs_low();
        spi_transfer(EEPROM_CMD_WRITE);
        spi_transfer((uint8_t)(eeprom_test_addr >> 8));
        spi_transfer((uint8_t)(eeprom_test_addr & 0xFFu));
        spi_transfer(eeprom_test_byte);
        eeprom_cs_high();                 /* the internal write starts on this rising edge */
        t_write_done = now;
        t_last_poll  = now;
        ee_state = EE_WAIT_READY;
        break;

    case EE_WAIT_READY:
        if ((uint32_t)(now - t_last_poll) < 1u)
        {
            break;                        /* next status check not due: return to the main loop */
        }
        t_last_poll = now;
        status = eeprom_read_status();
        if ((status & EEPROM_SR_RDY) == 0u)
        {
            eeprom_write_wait_ms = (uint32_t)(now - t_write_done);
            ee_state = EE_READ;
        }
        else if ((uint32_t)(now - t_write_done) >= EEPROM_WRITE_TIMEOUT_MS)
        {
            eeprom_timeout_count++;       /* EEPROM never reported ready */
            eeprom_verify_ok = 0u;
            ee_state = EE_FAILURE;
        }
        break;

    case EE_READ:
        ee_last_read = eeprom_read_byte(eeprom_test_addr);
        ee_state = EE_VERIFY;
        break;

    case EE_VERIFY:
        eeprom_read_value = ee_last_read;
        eeprom_verify_ok  = (ee_last_read == eeprom_test_byte) ? 1u : 0u;
        ee_state = (eeprom_verify_ok != 0u) ? EE_SUCCESS : EE_FAILURE;
        break;

    case EE_ABORT:
        eeprom_cs_low();                  /* clear the write-enable latch; bus is idle, CS high afterwards */
        spi_transfer(EEPROM_CMD_WRDI);
        eeprom_cs_high();
        ee_state = EE_IDLE;
        break;

    default:
        ee_state = EE_IDLE;
        break;
    }

    /* A PA0 press during a transaction must not queue up a second write. */
    if ((ee_state != EE_IDLE) && (ee_state != EE_SUCCESS) && (ee_state != EE_FAILURE))
    {
        btn_start_edge = 0u;
    }
}

void update_outputs(void)
{
    /* TODO 6.3  PB0..PB7 show ee_last_read (leds_write_byte). Green after a
     *           successful verification, red after a failed one
     *           (status_leds_show). Decide what the status LEDs should show
     *           while a transaction is in progress, and after an abort. */

    leds_write_byte(ee_last_read);

    switch (ee_state){

        case EE_SUCCESS: status_leds_show(STATUS_PASS); break;   /* green */
        case EE_FAILURE:    status_leds_show(STATUS_FAIL); break;   /* red   */
        case EE_IDLE:    break;      /* leave the boot result (or the last result) showing */
        default:         status_leds_show(STATUS_OFF);  break;   /* in progress or aborted: both off */
    }
}

/* ==========================================================================
 * RUN_TASK 6 - given
 *
 * The main loop has exactly the shape the handout asks for:
 *     read_inputs();  update_eeprom_state_machine();  update_outputs();
 *
 * Set ee_use_fsm = 0 in Live Expressions to run the Task 4 blocking path on
 * PA0 instead - useful when you explain why yours is non-blocking. PC13 keeps
 * toggling as a heartbeat; watch it on the scope in both modes.
 * ========================================================================== */

void task6_setup(void)
{
    task1_gpio_init();
    eeprom_spi_init();
    board_io_init();

    eeprom_read_only_path();
    ee_last_read = eeprom_read_value;

    /* TODO 6.4  Your state machine starts in its idle state. Make sure the
     *           boot-time result (eeprom_verify_ok) still shows on the status
     *           LEDs, so a reset still shows green for the persistence test. */

    ee_state = EE_IDLE;   /* eeprom_read_only_path() already put the boot result on the status LEDs;
                             update_outputs() leaves them alone while idle */
}

void task6_loop(uint32_t now)
{
    task1_gpio_update(now);
    read_inputs(now);

    if (ee_use_fsm)
    {
        update_eeprom_state_machine(now);
        update_outputs();
    }
    else
    {
        if (btn_start_edge)
        {
            btn_start_edge = 0u;
            eeprom_write_verify_path();     /* Task 4: blocks for the write */
            ee_last_read = eeprom_read_value;
        }
        btn_abort_edge = 0u;
    }
}
