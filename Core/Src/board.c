#include "board.h"

#include <string.h>

#include "app_config.h"
#include "led.h"

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
TIM_HandleTypeDef  htim6;
ADC_HandleTypeDef  hadc1;

static float battery_voltage;

static void system_clock_config(void)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};

  osc.OscillatorType  = RCC_OSCILLATORTYPE_HSE;
  osc.HSEState        = RCC_HSE_ON;
  osc.HSEPredivValue  = RCC_HSE_PREDIV_DIV1;
  osc.PLL.PLLState    = RCC_PLL_ON;
  osc.PLL.PLLSource   = RCC_PLLSOURCE_HSE;
  osc.PLL.PLLMUL      = RCC_PLL_MUL9;      /* 8 MHz * 9 = 72 MHz */

  if (HAL_RCC_OscConfig(&osc) != HAL_OK)
  {
    Error_Handler();
  }

  clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV2;       /* 36 MHz */
  clk.APB2CLKDivider = RCC_HCLK_DIV1;       /* 72 MHz */

  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
}

static void adc_init(void)
{
  ADC_ChannelConfTypeDef channel = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode    = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion       = 1U;

  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  channel.Channel      = BATT_ADC_CHANNEL;
  channel.Rank         = ADC_REGULAR_RANK_1;
  channel.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;

  if (HAL_ADC_ConfigChannel(&hadc1, &channel) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
}

static void uart_init(void)
{
  huart1.Instance          = USART1;
  huart1.Init.BaudRate     = UART_BAUDRATE;
  huart1.Init.WordLength   = UART_WORDLENGTH_8B;
  huart1.Init.StopBits     = UART_STOPBITS_1;
  huart1.Init.Parity       = UART_PARITY_NONE;
  huart1.Init.Mode         = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }

  huart2.Instance          = USART2;
  huart2.Init.BaudRate     = UART_BAUDRATE;
  huart2.Init.WordLength   = UART_WORDLENGTH_8B;
  huart2.Init.StopBits     = UART_STOPBITS_1;
  huart2.Init.Parity       = UART_PARITY_NONE;
  huart2.Init.Mode         = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void tim6_init(void)
{
  /* 72 MHz / (7199 + 1) = 10 kHz; period 99 -> 100 Hz update. */
  htim6.Instance               = TIM6;
  htim6.Init.Prescaler         = 7199U;
  htim6.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim6.Init.Period            = 99U;
  htim6.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim6.Init.RepetitionCounter = 0U;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_NVIC_SetPriority(TIM6_IRQn, 2U, 3U);
  HAL_NVIC_EnableIRQ(TIM6_IRQn);

  if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
}

void board_init(void)
{
  system_clock_config();
  led_init();
  adc_init();
  uart_init();
}

void board_start_tick(void)
{
  tim6_init();
}

void board_battery_update(void)
{
  uint32_t raw = 0U;

  /* Bounded polling instead of HAL_ADC_PollForConversion(): this runs from the
   * 100 Hz interrupt, where the SysTick-based timeout could not advance. */
  if (HAL_ADC_Start(&hadc1) == HAL_OK)
  {
    uint32_t guard = 100000U;

    while ((__HAL_ADC_GET_FLAG(&hadc1, ADC_FLAG_EOC) == RESET) && (guard > 0U))
    {
      guard--;
    }

    if (__HAL_ADC_GET_FLAG(&hadc1, ADC_FLAG_EOC) != RESET)
    {
      raw = HAL_ADC_GetValue(&hadc1);
    }

    (void)HAL_ADC_Stop(&hadc1);
  }

  battery_voltage = ((float)raw) * (BATT_VREF / BATT_ADC_FULL) * BATT_DIVIDER;
}

float board_battery_voltage(void)
{
  return battery_voltage;
}

void uart_send_both(const char *text)
{
  size_t length = strlen(text);

  if (length == 0U)
  {
    return;
  }

  (void)HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)length, 1000U);
  (void)HAL_UART_Transmit(&huart2, (uint8_t *)text, (uint16_t)length, 1000U);
}

void uart_send_usart1(const char *text)
{
  size_t length = strlen(text);

  if (length == 0U)
  {
    return;
  }

  (void)HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)length, 1000U);
}
