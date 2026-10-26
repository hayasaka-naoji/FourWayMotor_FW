/**
 * I2C2 slave with a small register interface (register-level, no HAL I2C).
 *
 * Write transaction (master -> board):
 *   [cmd][data_hi][data_lo]       for cmd 0..3
 *   [4]["67.0"]                   wheel diameter as ASCII
 *   [5][4 x int16 BE]             target speeds
 *   [6][4 x int16 BE]             direct PWM
 *
 * Read transaction (board -> master):
 *   master writes one register index, then reads 2 bytes (high, low).
 */

#ifndef I2C_SLAVE_H
#define I2C_SLAVE_H

void i2c_slave_init(void);

void i2c_slave_ev_irq(void);
void i2c_slave_er_irq(void);

#endif /* I2C_SLAVE_H */
