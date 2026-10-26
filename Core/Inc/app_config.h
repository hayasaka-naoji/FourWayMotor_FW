/**
 * Compile-time configuration: pinout, parameter page layout and limits.
 *
 * The values mirror the original FourWay_Motor_STM32 firmware v1.6.5 that
 * this project was reconstructed from.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/* Firmware identity                                                          */
/* -------------------------------------------------------------------------- */
#define FW_VERSION              "1.6.5"

/* -------------------------------------------------------------------------- */
/* Timing                                                                     */
/* -------------------------------------------------------------------------- */
#define CTRL_TICK_HZ            100U    /* TIM6 update rate                    */
#define BATTERY_TICK_DIV        20U     /* battery sampled every 200 ms        */
#define MSPD_TICK_DIV           5U      /* $MSPD frame every 50 ms             */

/* -------------------------------------------------------------------------- */
/* Parameter page (flash-emulated EEPROM)                                     */
/* -------------------------------------------------------------------------- */
#define PARAM_PAGE_BASE         0x08024000UL
#define PARAM_PAGE_SIZE         0x800UL         /* 2 KB page (high density)    */
#define PARAM_MAGIC_ADDR        (PARAM_PAGE_BASE + 0x02UL)
#define PARAM_MAGIC             0xAA55U

/* Field offsets are kept identical to the original firmware so that a board
 * already carrying parameters in flash keeps its calibration. */
#define PARAM_OFF_MTYPE         (PARAM_PAGE_BASE + 0x10UL)
#define PARAM_OFF_DEADZONE      (PARAM_PAGE_BASE + 0x14UL)
#define PARAM_OFF_MLINE         (PARAM_PAGE_BASE + 0x20UL)
#define PARAM_OFF_MPHASE        (PARAM_PAGE_BASE + 0x30UL)
#define PARAM_OFF_WDIAMETER     (PARAM_PAGE_BASE + 0x50UL)
#define PARAM_OFF_KP            (PARAM_PAGE_BASE + 0x80UL)
#define PARAM_OFF_KI            (PARAM_PAGE_BASE + 0x86UL)
#define PARAM_OFF_KD            (PARAM_PAGE_BASE + 0x92UL)

/* Defaults written by $flash_reset and on a blank device. */
#define DEF_MTYPE               1U
#define DEF_DEADZONE            1600U
#define DEF_MLINE               11U
#define DEF_MPHASE              30U
#define DEF_WDIAMETER           67.0f
#define DEF_KP                  0.8f
#define DEF_KI                  0.06f
#define DEF_KD                  0.5f

/* -------------------------------------------------------------------------- */
/* Control limits                                                             */
/* -------------------------------------------------------------------------- */
#define MTYPE_OPEN_LOOP         4U      /* 4 = no encoder, direct PWM only     */

#define DEADZONE_MAX            3600
#define SPEED_LIMIT             1000    /* mm/s                                */
#define PWM_LIMIT               3600    /* PWM counts (ARR + 1)                */
#define PID_OUT_LIMIT           2000.0f /* PID integrator/output clamp         */

#define PWM_PERIOD              3599U   /* 72 MHz / 3600 = 20 kHz              */
#define PWM_PRESCALER           0U

/* -------------------------------------------------------------------------- */
/* Pins                                                                       */
/* -------------------------------------------------------------------------- */
/* Status LED: PC13 (GPIOC ODR bit 13).  The original firmware drives it
 * through the bit-band alias 0x422201B4 and configures pin 13 as a 2 MHz
 * push-pull output (GPIO_Init at 0x08002D94).  Set LED_ACTIVE_HIGH to 0 if
 * your LED is wired active-low. */
#define LED_PORT                GPIOC
#define LED_PIN                 GPIO_PIN_13
#ifndef LED_ACTIVE_HIGH
#define LED_ACTIVE_HIGH         1
#endif
#define LED_CLK_ENABLE()        __HAL_RCC_GPIOC_CLK_ENABLE()

/* Optional second LED for "motion active" indication.  Uncomment the block
 * and set the port/pin to match the LED you wired.  The clock enable macro is
 * only needed when the LED is not on GPIOC. */
/* #define IND_LED_ENABLE */
/* #define IND_LED_PORT            GPIOC */
/* #define IND_LED_PIN             GPIO_PIN_5 */
/* #define IND_LED_ACTIVE_HIGH     1 */
/* #define IND_LED_CLK_ENABLE()    __HAL_RCC_GPIOC_CLK_ENABLE() */

/* Battery sense: ADC1 channel 14 = PC4, divider ratio ~4.03 */
#define BATT_ADC_CHANNEL        ADC_CHANNEL_14
#define BATT_DIVIDER            4.03f
#define BATT_VREF               3.3f
#define BATT_ADC_FULL           4096.0f

/* PWM (H-bridge) pins */
#define PWM_T1_CH1_PORT         GPIOA   /* PA8  - motor 2 leg A */
#define PWM_T1_CH1_PIN          GPIO_PIN_8
#define PWM_T1_CH2N_PORT        GPIOB   /* PB0  - motor 0 leg A */
#define PWM_T1_CH2N_PIN         GPIO_PIN_0
#define PWM_T1_CH3N_PORT        GPIOB   /* PB1  - motor 0 leg B */
#define PWM_T1_CH3N_PIN         GPIO_PIN_1
#define PWM_T1_CH4_PORT         GPIOA   /* PA11 - motor 2 leg B */
#define PWM_T1_CH4_PIN          GPIO_PIN_11

#define PWM_T8_CH1_PORT         GPIOC   /* PC6 - motor 1 leg A */
#define PWM_T8_CH1_PIN          GPIO_PIN_6
#define PWM_T8_CH2_PORT         GPIOC   /* PC7 - motor 1 leg B */
#define PWM_T8_CH2_PIN          GPIO_PIN_7
#define PWM_T8_CH3_PORT         GPIOC   /* PC8 - motor 3 leg A */
#define PWM_T8_CH3_PIN          GPIO_PIN_8
#define PWM_T8_CH4_PORT         GPIOC   /* PC9 - motor 3 leg B */
#define PWM_T8_CH4_PIN          GPIO_PIN_9

/* Encoder pins */
#define ENC_T2_CH1_PORT         GPIOA   /* PA15 (TIM2 partial remap 1) */
#define ENC_T2_CH1_PIN          GPIO_PIN_15
#define ENC_T2_CH2_PORT         GPIOB   /* PB3                          */
#define ENC_T2_CH2_PIN          GPIO_PIN_3

#define ENC_T3_CH1_PORT         GPIOA   /* PA6 */
#define ENC_T3_CH1_PIN          GPIO_PIN_6
#define ENC_T3_CH2_PORT         GPIOA   /* PA7 */
#define ENC_T3_CH2_PIN          GPIO_PIN_7

#define ENC_T4_CH1_PORT         GPIOB   /* PB6 */
#define ENC_T4_CH1_PIN          GPIO_PIN_6
#define ENC_T4_CH2_PORT         GPIOB   /* PB7 */
#define ENC_T4_CH2_PIN          GPIO_PIN_7

#define ENC_T5_CH1_PORT         GPIOA   /* PA0 */
#define ENC_T5_CH1_PIN          GPIO_PIN_0
#define ENC_T5_CH2_PORT         GPIOA   /* PA1 */
#define ENC_T5_CH2_PIN          GPIO_PIN_1

/* UART pins */
#define UART1_TX_PORT           GPIOA   /* PA9  */
#define UART1_TX_PIN            GPIO_PIN_9
#define UART1_RX_PORT           GPIOA   /* PA10 */
#define UART1_RX_PIN            GPIO_PIN_10

#define UART2_TX_PORT           GPIOA   /* PA2  */
#define UART2_TX_PIN            GPIO_PIN_2
#define UART2_RX_PORT           GPIOA   /* PA3  */
#define UART2_RX_PIN            GPIO_PIN_3

/* I2C2 pins (open drain) */
#define I2C2_SCL_PORT           GPIOB   /* PB10 */
#define I2C2_SCL_PIN            GPIO_PIN_10
#define I2C2_SDA_PORT           GPIOB   /* PB11 */
#define I2C2_SDA_PIN            GPIO_PIN_11

#define UART_BAUDRATE           115200U
#define I2C_SLAVE_ADDR_OAR1     0x4CU   /* 7-bit address 0x26                  */

/* -------------------------------------------------------------------------- */
/* Protocol limits                                                            */
/* -------------------------------------------------------------------------- */
#define CMD_BUF_SIZE            50U
#define RESP_BUF_SIZE           96U

#endif /* APP_CONFIG_H */
