/**
 * Board support: clock tree, ADC, UARTs and the 100 Hz time base.
 */

#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

#include "stm32f1xx_hal.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef  htim6;
extern ADC_HandleTypeDef  hadc1;

void board_init(void);
void board_start_tick(void);        /* start TIM6 after all modules are ready */

void board_battery_update(void);    /* sample ADC1_IN14 (PC4)             */
float board_battery_voltage(void);  /* last sampled battery voltage, V    */

void uart_send_both(const char *text);
void uart_send_usart1(const char *text);

void Error_Handler(void);

#endif /* BOARD_H */
