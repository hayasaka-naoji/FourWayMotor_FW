#include "encoder.h"

#include "app_config.h"
#include "params.h"
#include "stm32f1xx_hal.h"

#define ENC_MIDPOINT  0x7FFFU

static TIM_HandleTypeDef htim2;
static TIM_HandleTypeDef htim3;
static TIM_HandleTypeDef htim4;
static TIM_HandleTypeDef htim5;

/* Logic index -> hardware timer, matching the original firmware. */
static TIM_HandleTypeDef *const enc_handle[4] =
{
  &htim5,  /* index 0 */
  &htim3,  /* index 1 */
  &htim2,  /* index 2 */
  &htim4   /* index 3 */
};

static int32_t  enc_position[4];
static int16_t  enc_delta[4];

static void encoder_hw_init(TIM_HandleTypeDef *htim, TIM_TypeDef *instance)
{
  TIM_Encoder_InitTypeDef config = {0};

  htim->Instance = instance;
  htim->Init.Prescaler         = 0U;
  htim->Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim->Init.Period            = 0xFFFFU;
  htim->Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim->Init.RepetitionCounter = 0U;
  htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  config.EncoderMode  = TIM_ENCODERMODE_TI12;
  config.IC1Polarity  = TIM_ICPOLARITY_RISING;
  config.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  config.IC1Prescaler = TIM_ICPSC_DIV1;
  config.IC1Filter    = 10U;
  config.IC2Polarity  = TIM_ICPOLARITY_RISING;
  config.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  config.IC2Prescaler = TIM_ICPSC_DIV1;
  config.IC2Filter    = 10U;

  (void)HAL_TIM_Encoder_Init(htim, &config);

  __HAL_TIM_SET_COUNTER(htim, ENC_MIDPOINT);
  (void)HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL);
}

void encoder_init(void)
{
  encoder_hw_init(&htim5, TIM5);
  encoder_hw_init(&htim3, TIM3);
  encoder_hw_init(&htim2, TIM2);
  encoder_hw_init(&htim4, TIM4);

  encoder_reset_positions();
}

void encoder_reset_positions(void)
{
  uint32_t i;

  for (i = 0U; i < 4U; i++)
  {
    enc_position[i] = 0;
    enc_delta[i]    = 0;
    __HAL_TIM_SET_COUNTER(enc_handle[i], ENC_MIDPOINT);
  }
}

void encoder_update(void)
{
  uint32_t i;

  for (i = 0U; i < 4U; i++)
  {
    uint16_t counter = (uint16_t)__HAL_TIM_GET_COUNTER(enc_handle[i]);
    int16_t  delta   = (int16_t)((int32_t)ENC_MIDPOINT - (int32_t)counter);

    __HAL_TIM_SET_COUNTER(enc_handle[i], ENC_MIDPOINT);
    enc_delta[i] = delta;

    if (g_params.mtype == 2U)
    {
      /* mtype 2 inverts the counting convention; wheel 3 is opposite. */
      if (i < 3U)
      {
        enc_position[i] += delta;
      }
      else
      {
        enc_position[i] -= delta;
      }
    }
    else
    {
      if (i < 3U)
      {
        enc_position[i] -= delta;
      }
      else
      {
        enc_position[i] += delta;
      }
    }
  }
}

int32_t encoder_position(int index)
{
  if (index < 0 || index > 3)
  {
    return 0;
  }
  return enc_position[index];
}

int16_t encoder_delta(int index)
{
  if (index < 0 || index > 3)
  {
    return 0;
  }
  return enc_delta[index];
}
