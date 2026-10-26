/**
 * Four hardware quadrature encoders on TIM5 / TIM3 / TIM2 / TIM4.
 *
 * Logic index -> timer follows the original firmware:
 *   0 -> TIM5, 1 -> TIM3, 2 -> TIM2, 3 -> TIM4
 * Each 10 ms tick the counter is read as (0x7FFF - CNT) and reset to 0x7FFF,
 * which yields a signed delta in the range -32767..+32767.
 */

#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

void encoder_init(void);

/* Read all four timers; call from the 100 Hz control tick. */
void encoder_update(void);

int32_t encoder_position(int index);
int16_t encoder_delta(int index);
void    encoder_reset_positions(void);

#endif /* ENCODER_H */
