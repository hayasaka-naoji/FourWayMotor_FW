/**
 * Four H-bridge outputs (TIM1 + TIM8, 20 kHz) and the incremental PID.
 *
 * Motor -> half-bridge mapping (same as the original board):
 *   motor 0 : TIM1_CH2N (PB0) / TIM1_CH3N (PB1)
 *   motor 1 : TIM8_CH1  (PC6) / TIM8_CH2  (PC7)
 *   motor 2 : TIM1_CH1  (PA8) / TIM1_CH4  (PA11)
 *   motor 3 : TIM8_CH3  (PC8) / TIM8_CH4  (PC9)
 */

#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

extern float g_speed[4];       /* measured speed, mm/s                  */
extern float g_target[4];      /* target speed, mm/s                    */
extern float g_pid_output[4];  /* PID output, PWM counts (signed)       */

void motor_init(void);

/* 100 Hz control tick: encoder read -> speed -> PID -> PWM. */
void motor_control_tick(void);

void motor_pid_reset(void);
void motor_sync_speed_reference(void);

/* Command entry points (already clamped by the caller if needed). */
void motor_command_spd(const int16_t values[4]);
void motor_command_pwm(const int16_t values[4]);

/* Non-zero when the last $spd / $pwm command asks for motion.  Used by the
 * status LED; it reports the command, not the measured encoder speed. */
int motor_is_driving(void);

/* Low-level output stage: dead-zone compensation, clamp, CCR write. */
void motor_set_pwm(int motor, int32_t value);

#endif /* MOTOR_H */
