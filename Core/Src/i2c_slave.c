#include "i2c_slave.h"

#include <string.h>

#include "app_config.h"
#include "board.h"
#include "encoder.h"
#include "fmt.h"
#include "motor.h"
#include "params.h"
#include "stm32f1xx_hal.h"

#define I2C_RX_MAX   12U
#define I2C_TX_LEN   2U

static uint8_t  rx_buffer[I2C_RX_MAX];
static uint8_t  rx_count;
static uint8_t  reg_index;
static uint8_t  tx_buffer[I2C_TX_LEN];
static uint8_t  tx_index;
static uint8_t  tx_mode;

/* -------------------------------------------------------------------------- */
/* Register map                                                               */
/* -------------------------------------------------------------------------- */

static uint16_t register_read(uint8_t reg)
{
  switch (reg)
  {
    case 0x08U: /* battery voltage x10 */
      return (uint16_t)(board_battery_voltage() * 10.0f);

    case 0x10U: return (uint16_t)encoder_delta(0);
    case 0x11U: return (uint16_t)encoder_delta(1);
    case 0x12U: return (uint16_t)encoder_delta(2);
    case 0x13U: return (uint16_t)encoder_delta(3);

    case 0x20U: return (uint16_t)((uint32_t)encoder_position(0) >> 16);
    case 0x21U: return (uint16_t)encoder_position(0);
    case 0x22U: return (uint16_t)((uint32_t)encoder_position(1) >> 16);
    case 0x23U: return (uint16_t)encoder_position(1);
    case 0x24U: return (uint16_t)((uint32_t)encoder_position(2) >> 16);
    case 0x25U: return (uint16_t)encoder_position(2);
    case 0x26U: return (uint16_t)((uint32_t)encoder_position(3) >> 16);
    case 0x27U: return (uint16_t)encoder_position(3);

    default:
      return 0U;
  }
}

/* -------------------------------------------------------------------------- */
/* Write-command dispatch                                                     */
/* -------------------------------------------------------------------------- */

static uint16_t be16(const uint8_t *p)
{
  return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static int16_t be16_signed(const uint8_t *p)
{
  return (int16_t)be16(p);
}

static void dispatch_command(void)
{
  uint8_t cmd;
  uint16_t value;

  if (rx_count == 0U)
  {
    return;
  }

  cmd = rx_buffer[0];

  switch (cmd)
  {
    case 0U: /* mtype */
      if (rx_count >= 3U)
      {
        value = be16(&rx_buffer[1]);
        if (value >= 1U && value <= 4U)
        {
          g_params.mtype = value;
          (void)params_write_all();
          motor_sync_speed_reference();
        }
      }
      break;

    case 1U: /* deadzone */
      if (rx_count >= 3U)
      {
        value = be16(&rx_buffer[1]);
        if (value <= DEADZONE_MAX)
        {
          g_params.deadzone = value;
          (void)params_write_all();
        }
      }
      break;

    case 2U: /* mline */
      if (rx_count >= 3U)
      {
        g_params.mline = be16(&rx_buffer[1]);
        (void)params_write_all();
      }
      break;

    case 3U: /* mphase */
      if (rx_count >= 3U)
      {
        g_params.mphase = be16(&rx_buffer[1]);
        (void)params_write_all();
      }
      break;

    case 4U: /* wheel diameter, ASCII */
    {
      char text[8];
      uint8_t length = (uint8_t)(rx_count - 1U);
      float diameter;

      if (length > 6U)
      {
        length = 6U;
      }
      memcpy(text, &rx_buffer[1], length);
      text[length] = '\0';

      if (parse_f32(text, &diameter) == 0 && diameter > 0.0f)
      {
        g_params.wdiameter = diameter;
        (void)params_write_all();
      }
      break;
    }

    case 5U: /* target speeds */
      if (rx_count >= 9U)
      {
        int16_t speeds[4];
        uint32_t i;

        for (i = 0U; i < 4U; i++)
        {
          speeds[i] = be16_signed(&rx_buffer[1U + (i * 2U)]);
        }
        motor_command_spd(speeds);
      }
      break;

    case 6U: /* direct PWM */
      if (rx_count >= 9U)
      {
        int16_t pwms[4];
        uint32_t i;

        for (i = 0U; i < 4U; i++)
        {
          pwms[i] = be16_signed(&rx_buffer[1U + (i * 2U)]);
        }
        motor_command_pwm(pwms);
      }
      break;

    default:
      break;
  }
}

/* -------------------------------------------------------------------------- */
/* Initialisation                                                             */
/* -------------------------------------------------------------------------- */

void i2c_slave_init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_I2C2_CLK_ENABLE();

  __HAL_RCC_I2C2_FORCE_RESET();
  __HAL_RCC_I2C2_RELEASE_RESET();

  gpio.Pin   = I2C2_SCL_PIN | I2C2_SDA_PIN;
  gpio.Mode  = GPIO_MODE_AF_OD;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &gpio);

  /* 36 MHz APB1, 100 kHz standard mode. */
  I2C2->CR1   = 0U;
  I2C2->CR2   = 36U;
  I2C2->OAR1  = (uint16_t)I2C_SLAVE_ADDR_OAR1;
  I2C2->OAR2  = 0U;
  I2C2->CCR   = 180U;      /* 36 MHz / (2 * 100 kHz) */
  I2C2->TRISE = 37U;       /* 1000 ns * 36 MHz + 1  */
  I2C2->CR1   = I2C_CR1_PE;
  I2C2->CR1  |= I2C_CR1_ACK;
  I2C2->CR2  |= I2C_CR2_ITEVTEN | I2C_CR2_ITERREN | I2C_CR2_ITBUFEN;

  rx_count = 0U;
  tx_index = 0U;
  tx_mode  = 0U;
  reg_index = 0U;

  HAL_NVIC_SetPriority(I2C2_EV_IRQn, 3U, 0U);
  HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);
  HAL_NVIC_SetPriority(I2C2_ER_IRQn, 3U, 0U);
  HAL_NVIC_EnableIRQ(I2C2_ER_IRQn);
}

/* -------------------------------------------------------------------------- */
/* Interrupt handlers                                                         */
/* -------------------------------------------------------------------------- */

void i2c_slave_ev_irq(void)
{
  uint32_t sr1 = I2C2->SR1;

  if ((sr1 & I2C_SR1_ADDR) != 0U)
  {
    uint32_t sr2 = I2C2->SR2;   /* reading SR1 then SR2 clears ADDR */

    /* RXNE is needed again for this transfer. */
    I2C2->CR2 |= I2C_CR2_ITBUFEN;

    if ((sr2 & I2C_SR2_TRA) != 0U)
    {
      uint16_t value = register_read(reg_index);

      tx_buffer[0] = (uint8_t)(value >> 8);
      tx_buffer[1] = (uint8_t)(value & 0xFFU);
      tx_index = 0U;
      tx_mode  = 1U;
      I2C2->DR = tx_buffer[tx_index++];
    }
    else
    {
      tx_mode  = 0U;
      rx_count = 0U;
    }
  }

  /* Reading SR1/SR2 above cleared ADDR.  Refresh the snapshot so RXNE/TXE
   * reflect the current state: after the first transmit byte was written to
   * DR, TXE is already cleared and must not trigger a second write here. */
  sr1 = I2C2->SR1;

  if ((sr1 & I2C_SR1_RXNE) != 0U)
  {
    uint8_t data = (uint8_t)I2C2->DR;

    if (rx_count < I2C_RX_MAX)
    {
      rx_buffer[rx_count++] = data;
    }

    /* First byte of a write is also the register index for a following
     * repeated-start read. */
    if (rx_count == 1U)
    {
      reg_index = data;
    }
  }

  if ((sr1 & I2C_SR1_TXE) != 0U)
  {
    if (tx_mode != 0U)
    {
      if (tx_index < I2C_TX_LEN)
      {
        I2C2->DR = tx_buffer[tx_index++];
      }
      else
      {
        /* Both bytes queued; stop requesting buffer interrupts until the
         * master NACKs, otherwise TXE would keep the IRQ asserted. */
        I2C2->CR2 &= ~I2C_CR2_ITBUFEN;
      }
    }
  }

  if ((sr1 & I2C_SR1_STOPF) != 0U)
  {
    (void)I2C2->SR1;
    I2C2->CR1 |= I2C_CR1_PE;   /* write to CR1 clears STOPF */

    dispatch_command();
    rx_count = 0U;
    tx_mode  = 0U;

    I2C2->CR2 |= I2C_CR2_ITBUFEN;
  }
}

void i2c_slave_er_irq(void)
{
  uint32_t sr1 = I2C2->SR1;

  /* AF, BERR, ARLO, OVR, PECERR, TIMEOUT and SMBALERT are cleared by
   * writing 0 to them. */
  if ((sr1 & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO |
              I2C_SR1_OVR | I2C_SR1_PECERR | I2C_SR1_TIMEOUT |
              I2C_SR1_SMBALERT)) != 0U)
  {
    I2C2->SR1 = 0U;
  }

  if ((sr1 & I2C_SR1_AF) != 0U)
  {
    I2C2->CR1 |= I2C_CR1_ACK;
  }

  /* A bus error can leave the peripheral stuck; re-arm it. */
  if ((sr1 & (I2C_SR1_BERR | I2C_SR1_ARLO)) != 0U)
  {
    I2C2->CR1 &= ~I2C_CR1_PE;
    I2C2->CR1 |= I2C_CR1_PE;
  }

  tx_mode  = 0U;
  rx_count = 0U;
}
