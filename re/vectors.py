import struct, sys, json, collections

BASE = 0x08000000
data = open('fw.bin', 'rb').read()
CODE_LO, CODE_HI = BASE, BASE + len(data)

F1_IRQ = """WWDG PVD TAMPER RTC FLASH RCC EXTI0 EXTI1 EXTI2 EXTI3 EXTI4
DMA1_Channel1 DMA1_Channel2 DMA1_Channel3 DMA1_Channel4 DMA1_Channel5 DMA1_Channel6 DMA1_Channel7
ADC1_2 USB_HP_CAN1_TX USB_LP_CAN1_RX0 CAN1_RX1 CAN1_SCE EXTI9_5 TIM1_BRK TIM1_UP TIM1_TRG_COM TIM1_CC
TIM2 TIM3 TIM4 I2C1_EV I2C1_ER I2C2_EV I2C2_ER SPI1 SPI2 USART1 USART2 USART3 EXTI15_10 RTC_Alarm
USBWakeUp TIM8_BRK TIM8_UP TIM8_TRG_COM TIM8_CC ADC3 FSMC SDIO TIM5 SPI3 UART4 UART5 TIM6 TIM7
DMA2_Channel1 DMA2_Channel2 DMA2_Channel3 DMA2_Channel4_5""".split()

print("=== vector table ===")
sysvec = ["SP","Reset","NMI","HardFault","MemManage","BusFault","UsageFault","-","-","-","-",
          "SVC","DebugMon","-","PendSV","SysTick"]
words = struct.unpack_from('<64I', data, 0)
for i, w in enumerate(words):
    if w == 0:
        continue
    name = sysvec[i] if i < 16 else f"IRQ{i-16} {F1_IRQ[i-16] if i-16 < len(F1_IRQ) else '?'}"
    print(f"  [{i:2d}] {name:<28} 0x{w:08X}")

print("\n=== nonzero handler addresses (deduplicated) ===")
h = collections.Counter()
for i, w in enumerate(words):
    if i == 0 or w == 0:
        continue
    name = sysvec[i] if i < 16 else (F1_IRQ[i-16] if i-16 < len(F1_IRQ) else f'IRQ{i-16}')
    h[w & ~1] += 1
for a, n in h.most_common():
    inrange = CODE_LO <= a < CODE_HI
    print(f"  0x{a:08X} {'CODE' if inrange else 'OUT'}  used_by={n}")

print("\n=== first 0x100 bytes hex ===")
for off in range(0, 0x100, 16):
    print(f"  {BASE+off:08X}: {' '.join(f'{b:02X}' for b in data[off:off+16])}")
