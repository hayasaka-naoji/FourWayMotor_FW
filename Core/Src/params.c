#include "params.h"

#include <string.h>

#include "app_config.h"
#include "led.h"
#include "stm32f1xx_hal.h"

params_t g_params;

/* One page of staging RAM. The page is erased and rewritten as a whole,
 * exactly like the original implementation. */
static uint16_t page_buf[PARAM_PAGE_SIZE / 2U];

static uint16_t flash_read_u16(uint32_t address)
{
  return *(const volatile uint16_t *)address;
}

static float flash_read_f32(uint32_t address)
{
  const volatile uint8_t *src = (const volatile uint8_t *)address;
  uint8_t raw[4];
  float value;

  raw[0] = src[0];
  raw[1] = src[1];
  raw[2] = src[2];
  raw[3] = src[3];
  memcpy(&value, raw, sizeof(value));
  return value;
}

static void buf_put_u16(uint32_t offset, uint16_t value)
{
  page_buf[offset / 2U] = value;
}

static void buf_put_f32(uint32_t offset, float value)
{
  uint8_t raw[4];

  memcpy(raw, &value, sizeof(raw));
  page_buf[(offset / 2U) + 0U] = (uint16_t)raw[0] | ((uint16_t)raw[1] << 8);
  page_buf[(offset / 2U) + 1U] = (uint16_t)raw[2] | ((uint16_t)raw[3] << 8);
}

static void set_defaults(void)
{
  g_params.mtype     = DEF_MTYPE;
  g_params.deadzone  = DEF_DEADZONE;
  g_params.mline     = DEF_MLINE;
  g_params.mphase    = DEF_MPHASE;
  g_params.wdiameter = DEF_WDIAMETER;
  g_params.kp        = DEF_KP;
  g_params.ki        = DEF_KI;
  g_params.kd        = DEF_KD;
}

static void load_from_ram_copy(void)
{
  g_params.mtype     = flash_read_u16(PARAM_OFF_MTYPE);
  g_params.deadzone  = flash_read_u16(PARAM_OFF_DEADZONE);
  g_params.mline     = flash_read_u16(PARAM_OFF_MLINE);
  g_params.mphase    = flash_read_u16(PARAM_OFF_MPHASE);
  g_params.wdiameter = flash_read_f32(PARAM_OFF_WDIAMETER);
  g_params.kp        = flash_read_f32(PARAM_OFF_KP);
  g_params.ki        = flash_read_f32(PARAM_OFF_KI);
  g_params.kd        = flash_read_f32(PARAM_OFF_KD);
}

static int flash_float_equals(uint32_t address, float value)
{
  float stored = flash_read_f32(address);
  return (memcmp(&stored, &value, sizeof(float)) == 0) ? 1 : 0;
}

/* True when the page already holds exactly the values in g_params.  Writing
 * in that case would only burn an erase cycle and stall the control loop. */
static int page_holds_current_params(void)
{
  if (flash_read_u16(PARAM_MAGIC_ADDR) != PARAM_MAGIC)
  {
    return 0;
  }

  if ((flash_read_u16(PARAM_OFF_MTYPE)    != g_params.mtype) ||
      (flash_read_u16(PARAM_OFF_DEADZONE) != g_params.deadzone) ||
      (flash_read_u16(PARAM_OFF_MLINE)    != g_params.mline) ||
      (flash_read_u16(PARAM_OFF_MPHASE)   != g_params.mphase))
  {
    return 0;
  }

  return flash_float_equals(PARAM_OFF_WDIAMETER, g_params.wdiameter) &&
         flash_float_equals(PARAM_OFF_KP,        g_params.kp) &&
         flash_float_equals(PARAM_OFF_KI,        g_params.ki) &&
         flash_float_equals(PARAM_OFF_KD,        g_params.kd);
}

static int write_page(void)
{
  FLASH_EraseInitTypeDef erase;
  uint32_t page_error = 0U;
  uint32_t primask;
  uint32_t last = 0U;
  uint32_t i;
  int result = 0;

  memset(page_buf, 0xFF, sizeof(page_buf));
  buf_put_u16(2U, PARAM_MAGIC);

  buf_put_u16(PARAM_OFF_MTYPE    - PARAM_PAGE_BASE, g_params.mtype);
  buf_put_u16(PARAM_OFF_DEADZONE - PARAM_PAGE_BASE, g_params.deadzone);
  buf_put_u16(PARAM_OFF_MLINE    - PARAM_PAGE_BASE, g_params.mline);
  buf_put_u16(PARAM_OFF_MPHASE   - PARAM_PAGE_BASE, g_params.mphase);
  buf_put_f32(PARAM_OFF_WDIAMETER - PARAM_PAGE_BASE, g_params.wdiameter);
  buf_put_f32(PARAM_OFF_KP        - PARAM_PAGE_BASE, g_params.kp);
  buf_put_f32(PARAM_OFF_KI        - PARAM_PAGE_BASE, g_params.ki);
  buf_put_f32(PARAM_OFF_KD        - PARAM_PAGE_BASE, g_params.kd);

  /* Only program up to the last non-0xFFFF halfword. */
  for (i = (PARAM_PAGE_SIZE / 2U); i > 0U; i--)
  {
    if (page_buf[i - 1U] != 0xFFFFU)
    {
      last = i;
      break;
    }
  }

  /* Erasing stalls instruction fetch from flash: mask interrupts so that the
   * control loop cannot resume in the middle of the page rewrite. */
  primask = __get_PRIMASK();
  __disable_irq();
  led_busy_begin();

  HAL_FLASH_Unlock();

  erase.TypeErase   = FLASH_TYPEERASE_PAGES;
  erase.PageAddress = PARAM_PAGE_BASE;
  erase.NbPages     = 1U;

  if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK)
  {
    result = -1;
  }
  else
  {
    for (i = 0U; i < last; i++)
    {
      if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                            PARAM_PAGE_BASE + (i * 2U),
                            page_buf[i]) != HAL_OK)
      {
        result = -2;
        break;
      }
    }
  }

  HAL_FLASH_Lock();

  if (primask == 0U)
  {
    __enable_irq();
  }
  led_busy_end();

  if (result != 0)
  {
    led_signal_error();
  }

  return result;
}

void params_init(void)
{
  if (flash_read_u16(PARAM_MAGIC_ADDR) != PARAM_MAGIC)
  {
    set_defaults();
    (void)write_page();
  }

  load_from_ram_copy();
}

int params_write_all(void)
{
  if (page_holds_current_params())
  {
    return 0;
  }

  return write_page();
}

void params_factory_reset(void)
{
  set_defaults();
  (void)params_write_all();
}
