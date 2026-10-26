/**
 * system_stm32f1xx.c - minimal CMSIS system file for this project.
 *
 * The clock tree itself is configured by SystemClock_Config() in board.c;
 * this file only provides the CMSIS symbols the startup code and HAL expect.
 */

#include "stm32f1xx.h"

#if !defined(HSE_VALUE)
#define HSE_VALUE            8000000U
#endif
#if !defined(HSI_VALUE)
#define HSI_VALUE            8000000U
#endif

#define VECT_TAB_OFFSET      0x00000000U

uint32_t SystemCoreClock = 8000000U;

const uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
const uint8_t APBPrescTable[8]  = {0, 0, 0, 0, 1, 2, 3, 4};

void SystemInit(void)
{
  /* Reset the RCC clock configuration to its default state. */
  RCC->CR |= RCC_CR_HSION;
  RCC->CFGR &= 0xF8FF0000U;
  RCC->CR &= 0xFEF6FFFFU;
  RCC->CR &= 0xFFFBFFFFU;
  RCC->CFGR &= 0xFF80FFFFU;
  RCC->CIR = 0x009F0000U;

  SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;
}

void SystemCoreClockUpdate(void)
{
  uint32_t tmp;
  uint32_t pllmull;
  uint32_t pllsource;

  tmp = RCC->CFGR & RCC_CFGR_SWS;

  switch (tmp)
  {
    case 0x00U: /* HSI */
      SystemCoreClock = HSI_VALUE;
      break;

    case 0x04U: /* HSE */
      SystemCoreClock = HSE_VALUE;
      break;

    case 0x08U: /* PLL */
      pllmull   = (RCC->CFGR & RCC_CFGR_PLLMULL) >> 18;
      pllmull  += 2U;
      if (pllmull == 15U)
      {
        pllmull = 16U;
      }

      pllsource = RCC->CFGR & RCC_CFGR_PLLSRC;
      if (pllsource != 0U)
      {
        if ((RCC->CFGR & RCC_CFGR_PLLXTPRE) != 0U)
        {
          SystemCoreClock = (HSE_VALUE / 2U) * pllmull;
        }
        else
        {
          SystemCoreClock = HSE_VALUE * pllmull;
        }
      }
      else
      {
        SystemCoreClock = (HSI_VALUE / 2U) * pllmull;
      }
      break;

    default:
      SystemCoreClock = HSI_VALUE;
      break;
  }

  tmp = (RCC->CFGR & RCC_CFGR_HPRE) >> 4;
  SystemCoreClock >>= AHBPrescTable[tmp];
}
