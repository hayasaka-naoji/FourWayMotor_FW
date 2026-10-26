/**
 * Status LED indication.
 *
 * The board has one documented LED on PC13 (GPIOC ODR bit 13, active high by
 * default).  It is driven from the 100 Hz tick with patterns that describe the
 * controller state:
 *
 *   boot      three 100 ms flashes, then fall through to idle
 *   idle      one 50 ms blink every 2 s (alive, no motion commanded)
 *   motion    two 50 ms blinks every 1 s ($spd / $pwm target is non-zero)
 *   flash     steady on while the parameter page is erased/programmed
 *   error     fast 5 Hz blink for 2 s (rejected frame / flash write failure)
 *
 * An optional second LED for "motion active" can be enabled in app_config.h
 * with IND_LED_ENABLE / IND_LED_PORT / IND_LED_PIN.
 */

#ifndef LED_H
#define LED_H

#include <stdint.h>

void led_init(void);

/* Call from the 100 Hz tick.  driving != 0 selects the motion pattern. */
void led_update(uint8_t driving);

/* One-shot error indication (fast blink for ~2 s). */
void led_signal_error(void);

/* Hold the LED on while the parameter page is being rewritten.  The tick is
 * stalled during the flash operation, so the steady state stays visible. */
void led_busy_begin(void);
void led_busy_end(void);

#endif /* LED_H */
