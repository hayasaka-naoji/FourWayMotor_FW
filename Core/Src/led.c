#include "led.h"

#include "app_config.h"
#include "stm32f1xx_hal.h"

/* Timings are expressed in 100 Hz ticks (1 tick = 10 ms). */
#define LED_BOOT_END       45U     /* 450 ms: three short flashes          */
#define LED_ERROR_TICKS    200U    /* 2 s of fast blinking                 */

#if defined(IND_LED_ENABLE)
#ifndef IND_LED_PORT
#define IND_LED_PORT GPIOC
#endif
#ifndef IND_LED_PIN
#define IND_LED_PIN GPIO_PIN_5
#endif
#ifndef IND_LED_ACTIVE_HIGH
#define IND_LED_ACTIVE_HIGH 1
#endif
#ifndef IND_LED_CLK_ENABLE
#define IND_LED_CLK_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()
#endif
#endif

static uint32_t ticks;
static volatile uint16_t error_ticks;
static volatile uint8_t  busy;
static uint8_t  booted;

static void status_led_write(uint8_t on)
{
#if LED_ACTIVE_HIGH
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
#endif
}

#if defined(IND_LED_ENABLE)
static void ind_led_write(uint8_t on)
{
  HAL_GPIO_WritePin(IND_LED_PORT, IND_LED_PIN,
                    (on == (uint8_t)IND_LED_ACTIVE_HIGH) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
#endif

void led_init(void)
{
  GPIO_InitTypeDef gpio = {0};

  LED_CLK_ENABLE();

  gpio.Pin   = LED_PIN;
  gpio.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_PORT, &gpio);
  status_led_write(0U);

#if defined(IND_LED_ENABLE)
  IND_LED_CLK_ENABLE();
  gpio.Pin = IND_LED_PIN;
  HAL_GPIO_Init(IND_LED_PORT, &gpio);
  ind_led_write(0U);
#endif

  ticks       = 0U;
  error_ticks = 0U;
  busy        = 0U;
  booted      = 0U;
}

void led_signal_error(void)
{
  error_ticks = LED_ERROR_TICKS;
}

void led_busy_begin(void)
{
  busy = 1U;
  status_led_write(1U);
}

void led_busy_end(void)
{
  busy = 0U;
}

void led_update(uint8_t driving)
{
  uint8_t  on;
  uint32_t phase;

  ticks++;

  if (busy != 0U)
  {
    on = 1U;
  }
  else if (error_ticks != 0U)
  {
    error_ticks--;
    on = ((ticks % 20U) < 10U) ? 1U : 0U;          /* 5 Hz */
  }
  else if (booted == 0U)
  {
    on = ((ticks < 10U) || (ticks >= 15U && ticks < 25U) ||
          (ticks >= 30U && ticks < 40U)) ? 1U : 0U;

    if (ticks >= LED_BOOT_END)
    {
      booted = 1U;
    }
  }
  else if (driving != 0U)
  {
    phase = ticks % 100U;                          /* 1 s cycle */
    on = ((phase < 5U) || (phase >= 15U && phase < 20U)) ? 1U : 0U;
  }
  else
  {
    on = ((ticks % 200U) < 5U) ? 1U : 0U;          /* 50 ms every 2 s */
  }

  status_led_write(on);

#if defined(IND_LED_ENABLE)
  ind_led_write((uint8_t)((driving != 0U) && (booted != 0U)));
#endif
}
