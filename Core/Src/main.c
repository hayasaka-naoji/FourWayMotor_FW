/**
 * FourWay motor controller - STM32F103RCTx / STM32CubeIDE
 *
 * Reimplementation of the original FourWay_Motor_STM32 firmware v1.6.5:
 *   4x quadrature encoder (TIM2/3/4/5)
 *   4x H-bridge PWM       (TIM1 + TIM8, 20 kHz)
 *   incremental PID       (100 Hz, TIM6)
 *   ASCII protocol        (USART1 + USART2, 115200)
 *   I2C2 slave register interface
 *   parameters in flash   (0x08024000)
 */

#include "board.h"

#include "app_config.h"
#include "encoder.h"
#include "i2c_slave.h"
#include "led.h"
#include "motor.h"
#include "params.h"
#include "protocol.h"
#include "stm32f1xx_hal.h"

static uint16_t battery_divider;
static uint16_t mspd_divider;

int main(void)
{
  HAL_Init();
  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

  board_init();

  params_init();
  encoder_init();
  motor_init();
  i2c_slave_init();
  protocol_init();

  uart_send_usart1(" Motor_Version:" FW_VERSION "\r\n");

  battery_divider = 0U;
  mspd_divider    = 0U;

  /* Start the 100 Hz control tick only once every module is initialised. */
  board_start_tick();

  for (;;)
  {
    protocol_poll();
    protocol_telemetry();
  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance != TIM6)
  {
    return;
  }

  motor_control_tick();

  battery_divider++;
  if (battery_divider >= BATTERY_TICK_DIV)
  {
    battery_divider = 0U;
    board_battery_update();
  }

  mspd_divider++;
  if (mspd_divider >= MSPD_TICK_DIV)
  {
    mspd_divider = 0U;
    protocol_set_mspd_flag();
  }

  led_update((uint8_t)motor_is_driving());
}

void Error_Handler(void)
{
  __disable_irq();
  for (;;)
  {
  }
}
