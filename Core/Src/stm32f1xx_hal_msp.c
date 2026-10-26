/**
 * HAL MSP: clocks, GPIO alternate functions and NVIC setup.
 */

#include "board.h"

#include "app_config.h"
#include "stm32f1xx_hal.h"

static void gpio_af_pp(GPIO_TypeDef *port, uint16_t pin)
{
  GPIO_InitTypeDef gpio = {0};

  gpio.Pin   = pin;
  gpio.Mode  = GPIO_MODE_AF_PP;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(port, &gpio);
}

static void gpio_input(GPIO_TypeDef *port, uint16_t pin)
{
  GPIO_InitTypeDef gpio = {0};

  gpio.Pin  = pin;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(port, &gpio);
}

void HAL_MspInit(void)
{
  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();
}

/* -------------------------------------------------------------------------- */
/* UART                                                                       */
/* -------------------------------------------------------------------------- */

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    gpio_af_pp(UART1_TX_PORT, UART1_TX_PIN);
    gpio_input(UART1_RX_PORT, UART1_RX_PIN);

    HAL_NVIC_SetPriority(USART1_IRQn, 3U, 0U);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  }
  else if (huart->Instance == USART2)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    gpio_af_pp(UART2_TX_PORT, UART2_TX_PIN);
    gpio_input(UART2_RX_PORT, UART2_RX_PIN);

    HAL_NVIC_SetPriority(USART2_IRQn, 3U, 0U);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
  }
}

/* -------------------------------------------------------------------------- */
/* PWM timers                                                                 */
/* -------------------------------------------------------------------------- */

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_TIM1_CLK_ENABLE();

    /* Partial remap moves CH1N/CH2N/CH3N to PA7/PB0/PB1. */
    __HAL_AFIO_REMAP_TIM1_PARTIAL();

    gpio_af_pp(PWM_T1_CH1_PORT,  PWM_T1_CH1_PIN);
    gpio_af_pp(PWM_T1_CH2N_PORT, PWM_T1_CH2N_PIN);
    gpio_af_pp(PWM_T1_CH3N_PORT, PWM_T1_CH3N_PIN);
    gpio_af_pp(PWM_T1_CH4_PORT,  PWM_T1_CH4_PIN);
  }
  else if (htim->Instance == TIM8)
  {
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_TIM8_CLK_ENABLE();

    gpio_af_pp(PWM_T8_CH1_PORT, PWM_T8_CH1_PIN);
    gpio_af_pp(PWM_T8_CH2_PORT, PWM_T8_CH2_PIN);
    gpio_af_pp(PWM_T8_CH3_PORT, PWM_T8_CH3_PIN);
    gpio_af_pp(PWM_T8_CH4_PORT, PWM_T8_CH4_PIN);
  }
}

/* -------------------------------------------------------------------------- */
/* Encoder timers                                                             */
/* -------------------------------------------------------------------------- */

void HAL_TIM_Encoder_MspInit(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* Free PA15/PB3 from JTAG, then select TIM2 partial remap 1. */
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
    __HAL_AFIO_REMAP_TIM2_PARTIAL_1();

    gpio_input(ENC_T2_CH1_PORT, ENC_T2_CH1_PIN);
    gpio_input(ENC_T2_CH2_PORT, ENC_T2_CH2_PIN);
  }
  else if (htim->Instance == TIM3)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    gpio_input(ENC_T3_CH1_PORT, ENC_T3_CH1_PIN);
    gpio_input(ENC_T3_CH2_PORT, ENC_T3_CH2_PIN);
  }
  else if (htim->Instance == TIM4)
  {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();

    gpio_input(ENC_T4_CH1_PORT, ENC_T4_CH1_PIN);
    gpio_input(ENC_T4_CH2_PORT, ENC_T4_CH2_PIN);
  }
  else if (htim->Instance == TIM5)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM5_CLK_ENABLE();

    gpio_input(ENC_T5_CH1_PORT, ENC_T5_CH1_PIN);
    gpio_input(ENC_T5_CH2_PORT, ENC_T5_CH2_PIN);
  }
}

/* -------------------------------------------------------------------------- */
/* ADC / time base                                                            */
/* -------------------------------------------------------------------------- */

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
  RCC_PeriphCLKInitTypeDef clk = {0};
  GPIO_InitTypeDef gpio = {0};

  if (hadc->Instance != ADC1)
  {
    return;
  }

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_ADC1_CLK_ENABLE();

  /* ADC clock = PCLK2 / 6 = 12 MHz */
  clk.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  clk.AdcClockSelection    = RCC_ADCPCLK2_DIV6;
  (void)HAL_RCCEx_PeriphCLKConfig(&clk);

  gpio.Pin  = GPIO_PIN_4;
  gpio.Mode = GPIO_MODE_ANALOG;
  HAL_GPIO_Init(GPIOC, &gpio);
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6)
  {
    __HAL_RCC_TIM6_CLK_ENABLE();
  }
}
