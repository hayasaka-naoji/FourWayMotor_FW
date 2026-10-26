/**
 * Interrupt handlers.
 */

#include "board.h"

#include "i2c_slave.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_it.h"

/* -------------------------------------------------------------------------- */
/* Cortex-M3 exceptions                                                       */
/* -------------------------------------------------------------------------- */

void NMI_Handler(void)
{
  for (;;)
  {
  }
}

void HardFault_Handler(void)
{
  for (;;)
  {
  }
}

void MemManage_Handler(void)
{
  for (;;)
  {
  }
}

void BusFault_Handler(void)
{
  for (;;)
  {
  }
}

void UsageFault_Handler(void)
{
  for (;;)
  {
  }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}

/* -------------------------------------------------------------------------- */
/* Peripheral interrupts                                                      */
/* -------------------------------------------------------------------------- */

void USART1_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart1);
}

void USART2_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart2);
}

void TIM6_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim6);
}

void I2C2_EV_IRQHandler(void)
{
  i2c_slave_ev_irq();
}

void I2C2_ER_IRQHandler(void)
{
  i2c_slave_er_irq();
}
