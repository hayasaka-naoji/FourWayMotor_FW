/**
 * ASCII command protocol on USART1 + USART2 and periodic telemetry.
 *
 * Frame:  $<name> <args>#        e.g. $mtype 1#   $spd 300,-300,300,-300#
 * Reply:  plain text terminated by CR/LF, sent on both UARTs.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

void protocol_init(void);

/* Feed one received byte (called from the UART interrupt). */
void protocol_rx_byte(uint8_t byte);

/* Execute a pending command / emit pending telemetry (main loop). */
void protocol_poll(void);
void protocol_telemetry(void);

/* Set by the 100 Hz tick every 50 ms to throttle $MSPD. */
void protocol_set_mspd_flag(void);

#endif /* PROTOCOL_H */
