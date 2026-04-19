

#include <avr/io.h>

/*
 * TWI (I2C) driver for ATxmega128A3U — TWIC, 32 MHz, ~100 kHz
 *
 * BAUD = (F_CPU / (2 * F_SCL)) - 5
 *      = (32000000 / 200000) - 5 = 155
 *
 * PC0 = SDA, PC1 = SCL (set as wired-and inputs, pull-ups via external resistors)
 */

#define TWI_SLAVE_ADDR  0x5F
#define TWI_BAUD_VAL    155

/* Wait for master write interrupt flag, then check ACK */
static uint8_t twi_wait_ack(void)
{
    while (!(TWIC.MASTER.STATUS & TWI_MASTER_WIF_bm))
        ;

    /* RXACK = 0 means ACK received from slave */
    if (TWIC.MASTER.STATUS & TWI_MASTER_RXACK_bm)
        return 0; /* NACK — slave not responding */

    return 1;
}

void twi_init(void)
{
    /* PC0 (SDA) and PC1 (SCL): wired-and (open-drain) + no pull-up driven by MCU
     * External pull-up resistors are required on both lines.
     * PORTC pins default to input — just configure wired-and output for the TWI
     * peripheral to drive them low. */
     
    //PORTC.PIN0CTRL = PORT_OPC_WIREDAND_gc;
    //PORTC.PIN1CTRL = PORT_OPC_WIREDAND_gc;

    /* Set baud rate for ~100 kHz */
    TWIC.MASTER.BAUD = TWI_BAUD_VAL;

    /* Enable master, wire interrupt on write, read */
    TWIC.MASTER.CTRLA = TWI_MASTER_ENABLE_bm
                      | TWI_MASTER_WIEN_bm
                      | TWI_MASTER_RIEN_bm;

    /* Force bus state to IDLE so the master can initiate transfers */
    TWIC.MASTER.STATUS = TWI_MASTER_BUSSTATE_IDLE_gc;
}

/*
 * Send a single byte to the slave.
 * Returns 0 on success, -1 on NACK/error.
 */
uint8_t twi_send_byte(uint8_t data)
{
    /* Send START + SLA+W (address shifted left, R/W = 0) */
    TWIC.MASTER.ADDR = (TWI_SLAVE_ADDR << 1) | 0x00;

    if (twi_wait_ack() != 0)
    {
        TWIC.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
        return 0;
    }

    /* Send data byte */
    TWIC.MASTER.DATA = data;

    if (twi_wait_ack() != 0)
    {
        TWIC.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;
        return 0;
    }

    /* Issue STOP */
    TWIC.MASTER.CTRLC = TWI_MASTER_CMD_STOP_gc;

    return 1;
}

/*
 * Read a single byte from the slave.
 * Returns the received byte, or -1 on error.
 */
uint8_t twi_read_byte(void)
{
    /* Send START + SLA+R (address shifted left, R/W = 1) */
    TWIC.MASTER.ADDR = (TWI_SLAVE_ADDR << 1) | 0x01;

    /* Wait for master read interrupt flag (byte received) */
    while (!(TWIC.MASTER.STATUS & TWI_MASTER_RIF_bm))
    {
        /* Also bail if arbitration lost or bus error */
        if (TWIC.MASTER.STATUS & (TWI_MASTER_ARBLOST_bm | TWI_MASTER_BUSERR_bm))
            return 0;
    }

    uint8_t received = TWIC.MASTER.DATA;

    /* Send NACK + STOP (reading only 1 byte, so we NACK to end the transfer) */
    TWIC.MASTER.CTRLC = TWI_MASTER_ACKACT_bm | TWI_MASTER_CMD_STOP_gc;

    return received;
}

uint8_t keyboard_init()
{
    twi_init();
    return 1;
}

uint8_t keyboard_get_key()
{
    return twi_read_byte();
}

uint8_t keyboard_wait_key()
{
    uint8_t a = 0;
    while(a == 0)
    {
        a = twi_read_byte();
    }
    return a;
}