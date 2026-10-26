#include "motor.h"

#include <string.h>

#include "app_config.h"
#include "encoder.h"
#include "params.h"
#include "stm32f1xx_hal.h"

#define PI_F  3.14159265f

typedef struct
{
  float target;
  float output;
  float e1;   /* e[n-1] */
  float e2;   /* e[n-2] */
} pid_state_t;

static TIM_HandleTypeDef htim1;
static TIM_HandleTypeDef htim8;

static pid_state_t pid[4];
static int32_t prev_position[4];
static uint8_t closed_loop;
static uint8_t driving;

/* The counts -> mm/s conversion is precomputed instead of dividing on every
 * 100 Hz tick.  It is refreshed only when the encoder/wheel parameters change. */
static float speed_scale;
static uint16_t cached_mline;
static uint16_t cached_mphase;
static float cached_diameter;
static uint8_t scale_valid;

float g_speed[4];
float g_target[4];
float g_pid_output[4];

static void pwm_hw_init(TIM_HandleTypeDef *htim, TIM_TypeDef *instance)
{
  TIM_OC_InitTypeDef oc = {0};

  htim->Instance = instance;
  htim->Init.Prescaler         = PWM_PRESCALER;
  htim->Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim->Init.Period            = PWM_PERIOD;
  htim->Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim->Init.RepetitionCounter = 0U;
  htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

  (void)HAL_TIM_PWM_Init(htim);

  oc.OCMode       = TIM_OCMODE_PWM1;
  oc.Pulse        = 0U;
  oc.OCPolarity   = TIM_OCPOLARITY_HIGH;
  oc.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
  oc.OCFastMode   = TIM_OCFAST_DISABLE;
  oc.OCIdleState  = TIM_OCIDLESTATE_RESET;
  oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;

  (void)HAL_TIM_PWM_ConfigChannel(htim, &oc, TIM_CHANNEL_1);
  (void)HAL_TIM_PWM_ConfigChannel(htim, &oc, TIM_CHANNEL_2);
  (void)HAL_TIM_PWM_ConfigChannel(htim, &oc, TIM_CHANNEL_3);
  (void)HAL_TIM_PWM_ConfigChannel(htim, &oc, TIM_CHANNEL_4);
}

void motor_init(void)
{
  pwm_hw_init(&htim1, TIM1);
  pwm_hw_init(&htim8, TIM8);

  /* TIM1 channels 1 and 4 drive the main outputs (PA8 / PA11); channels 2
   * and 3 drive only their complementary outputs (PB0 / PB1). */
  (void)HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  (void)HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
  (void)HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_3);
  (void)HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);

  (void)HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
  (void)HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);
  (void)HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3);
  (void)HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4);

  motor_pid_reset();
  motor_sync_speed_reference();

  /* Start with all bridges at 0 % (TIM1 N outputs idle high -> brake). */
  motor_set_pwm(0, 0);
  motor_set_pwm(1, 0);
  motor_set_pwm(2, 0);
  motor_set_pwm(3, 0);
}

void motor_pid_reset(void)
{
  uint32_t i;

  memset(pid, 0, sizeof(pid));
  for (i = 0U; i < 4U; i++)
  {
    g_target[i]     = 0.0f;
    g_pid_output[i] = 0.0f;
    g_speed[i]      = 0.0f;
  }
  closed_loop = 0U;
  driving     = 0U;
}

void motor_sync_speed_reference(void)
{
  uint32_t i;
  uint32_t primask;

  primask = __get_PRIMASK();
  __disable_irq();

  for (i = 0U; i < 4U; i++)
  {
    prev_position[i] = encoder_position((int)i);
  }

  if (primask == 0U)
  {
    __enable_irq();
  }
}

int motor_is_driving(void)
{
  /* With mtype 4 the control tick is skipped entirely, so a leftover $spd
   * target must not keep the run indicator lit.  Direct $pwm still drives. */
  if ((closed_loop != 0U) && (g_params.mtype == MTYPE_OPEN_LOOP))
  {
    return 0;
  }

  return (driving != 0U) ? 1 : 0;
}

void motor_set_pwm(int motor, int32_t value)
{
  uint32_t magnitude;

  if (motor < 0 || motor > 3)
  {
    return;
  }

  /* Dead-zone compensation: add the configured offset in the drive direction. */
  if (value > 0)
  {
    value += (int32_t)g_params.deadzone;
  }
  else if (value < 0)
  {
    value -= (int32_t)g_params.deadzone;
  }
  else
  {
    value = 0;
  }

  if (value > PWM_LIMIT)
  {
    value = PWM_LIMIT;
  }
  else if (value < -PWM_LIMIT)
  {
    value = -PWM_LIMIT;
  }

  magnitude = (value >= 0) ? (uint32_t)value : (uint32_t)(-value);

  switch (motor)
  {
    case 0: /* TIM1 CH2N / CH3N */
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (value >= 0) ? 0U : magnitude);
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (value >= 0) ? magnitude : 0U);
      break;

    case 1: /* TIM8 CH1 / CH2 */
      __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, (value >= 0) ? 0U : magnitude);
      __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, (value >= 0) ? magnitude : 0U);
      break;

    case 2: /* TIM1 CH1 / CH4 */
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (value >= 0) ? 0U : magnitude);
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, (value >= 0) ? magnitude : 0U);
      break;

    default: /* TIM8 CH3 / CH4 */
      __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, (value >= 0) ? 0U : magnitude);
      __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, (value >= 0) ? magnitude : 0U);
      break;
  }
}

static void speed_scale_update(void)
{
  float counts_per_rev;

  if ((scale_valid != 0U) &&
      (cached_mline == g_params.mline) &&
      (cached_mphase == g_params.mphase) &&
      (cached_diameter == g_params.wdiameter))
  {
    return;
  }

  cached_mline    = g_params.mline;
  cached_mphase   = g_params.mphase;
  cached_diameter = g_params.wdiameter;

  counts_per_rev = (float)cached_mline * (float)cached_mphase * 4.0f;
  if (counts_per_rev < 1.0f)
  {
    counts_per_rev = 1.0f;
  }

  /* (counts per 10 ms) -> mm/s */
  speed_scale = 100.0f * PI_F * cached_diameter / counts_per_rev;
  scale_valid = 1U;
}

static void speed_calculate(void)
{
  uint32_t i;

  speed_scale_update();

  for (i = 0U; i < 4U; i++)
  {
    int32_t position = encoder_position((int)i);
    int32_t delta    = (int32_t)((uint32_t)position - (uint32_t)prev_position[i]);

    prev_position[i] = position;
    g_speed[i] = (float)delta * speed_scale;
  }
}

static float clampf(float value, float limit)
{
  if (value > limit)
  {
    return limit;
  }
  if (value < -limit)
  {
    return -limit;
  }
  return value;
}

static void pid_update(void)
{
  uint32_t i;

  for (i = 0U; i < 4U; i++)
  {
    float e  = g_target[i] - g_speed[i];
    float du = g_params.kp * (e - pid[i].e1)
             + g_params.ki * e
             + g_params.kd * (e - 2.0f * pid[i].e1 + pid[i].e2);

    pid[i].output = clampf(pid[i].output + du, PID_OUT_LIMIT);
    pid[i].e2     = pid[i].e1;
    pid[i].e1     = e;

    g_pid_output[i] = pid[i].output;
    motor_set_pwm((int)i, (int32_t)pid[i].output);
  }
}

void motor_control_tick(void)
{
  if (g_params.mtype == MTYPE_OPEN_LOOP)
  {
    return;
  }

  encoder_update();
  speed_calculate();

  if (closed_loop == 0U)
  {
    return;
  }

  pid_update();
}

void motor_command_spd(const int16_t values[4])
{
  uint32_t i;
  uint8_t any = 0U;

  for (i = 0U; i < 4U; i++)
  {
    float target = (float)values[i];

    if (target > (float)SPEED_LIMIT)
    {
      target = (float)SPEED_LIMIT;
    }
    else if (target < -(float)SPEED_LIMIT)
    {
      target = -(float)SPEED_LIMIT;
    }

    if (target != 0.0f)
    {
      any = 1U;
    }

    g_target[i]     = target;
    pid[i].target   = target;
  }

  closed_loop = 1U;
  driving     = any;
}

void motor_command_pwm(const int16_t values[4])
{
  uint32_t i;
  uint8_t any = 0U;

  /* Direct PWM overrides the closed loop until the next $spd command. */
  closed_loop = 0U;

  for (i = 0U; i < 4U; i++)
  {
    if (values[i] != 0)
    {
      any = 1U;
    }
    motor_set_pwm((int)i, (int32_t)values[i]);
  }

  driving = any;
}
