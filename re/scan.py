import struct, re, collections

BASE = 0x08000000
data = open('fw.bin','rb').read()
N = len(data)

def rd32(off): return struct.unpack_from('<I', data, off)[0]

# ---- 1. collect all 32-bit words, look for peripheral-ish addresses ----
words = collections.Counter()
for off in range(0, N-3, 4):
    w = rd32(off)
    if 0x40000000 <= w < 0x60000000:
        words[w] += 1

def periph(w):
    # Cortex-M3/M4 STM32 memory map
    if w == 0xE000ED00: return "SCB"
    if 0xE000E010 <= w <= 0xE000E01F: return "SysTick"
    if 0xE000E100 <= w <= 0xE000E11F: return "NVIC ISER"
    if 0xE000E180 <= w <= 0xE000E19F: return "NVIC ICER"
    if 0xE000E200 <= w <= 0xE000E21F: return "NVIC ISPR"
    if 0xE000E280 <= w <= 0xE000E29F: return "NVIC ICPR"
    if 0xE000ED00 <= w <= 0xE000ED8F: return "SCB"
    if w == 0xE000ED88: return "SCB->CPACR"
    if w == 0xE000ED08: return "SCB->VTOR"
    g = w & 0xFFFFF000
    if g == 0x40010800: return "GPIOA"
    if g == 0x40010C00: return "GPIOB"
    if g == 0x40011000: return "GPIOC"
    if g == 0x40011400: return "GPIOD"
    if g == 0x40011800: return "GPIOE"
    if g == 0x40011C00: return "GPIOF"
    if g == 0x40012000: return "GPIOG"
    if g == 0x40012400: return "AFIO"
    if g == 0x40020000: return "GPIOA(F4)"
    if g == 0x40020400: return "GPIOB(F4)"
    if g == 0x40020800: return "GPIOC(F4)"
    if g == 0x40020C00: return "GPIOD(F4)"
    if g == 0x40021000: return "RCC(F1)"
    if g == 0x40023800: return "RCC(F4)"
    if g == 0x40010000: return "DBGMCU"
    if g == 0x40013800: return "USART1"
    if g == 0x40004400: return "USART2"
    if g == 0x40004800: return "USART3"
    if g == 0x40007000: return "ADC1"
    if g == 0x40007400: return "ADC2"
    if g == 0x40005400: return "I2C1"
    if g == 0x40005800: return "I2C2"
    if g == 0x40013000: return "SPI1"
    if g == 0x40003800: return "SPI2"
    if g == 0x40012C00: return "TIM1"
    if g == 0x40000000: return "TIM2"
    if g == 0x40000400: return "TIM3"
    if g == 0x40000800: return "TIM4"
    if g == 0x40000C00: return "TIM5"
    if g == 0x40001000: return "TIM6"
    if g == 0x40001400: return "TIM7"
    if g == 0x40013400: return "TIM8"
    if g == 0x40022000: return "FLASH"
    if g == 0x40012400: return "AFIO"
    if g == 0x40012000: return "EXTI"
    if 0x40006C00 <= w < 0x40008000: return "ADC"
    if 0x40003C00 <= w < 0x40003C10: return "CAN?"
    return None

print("=== peripheral register addresses used ===")
seen = set()
for w, n in words.most_common():
    p = periph(w)
    if p and (p, w) not in seen:
        seen.add((p, w))
        print(f"  0x{w:08X}  {p:<14} x{n}")
print()
# group by peripheral block
blk = collections.Counter()
for w, n in words.items():
    g = w & 0xFFFFFC00
    p = periph(g)
    if p: blk[p] += n
print("=== peripheral blocks (approx) ===")
for p, n in blk.most_common():
    print(f"  {p:<14} refs~{n}")

# ---- 2. strings ----
print("\n=== ASCII strings (len>=4) ===")
for m in re.finditer(rb'[\x20-\x7e]{4,}', data):
    s = m.group().decode()
    print(f"  0x{BASE+m.start():08X}  {s}")

print("\n=== UTF-16LE strings ===")
for m in re.finditer(rb'(?:[\x20-\x7e]\x00){4,}', data):
    print(f"  0x{BASE+m.start():08X}  {m.group().decode('utf-16le')}")

print("\n=== UTF-8 Chinese strings ===")
for m in re.finditer(rb'(?:[\xe4-\xe9][\x80-\xbf]{2}){2,}', data):
    try:
        print(f"  0x{BASE+m.start():08X}  {m.group().decode('utf-8')}")
    except Exception: pass

print("\n=== GBK Chinese strings (rough) ===")
for m in re.finditer(rb'(?:[\xb0-\xf7][\xa1-\xfe]){2,}', data):
    try:
        print(f"  0x{BASE+m.start():08X}  {m.group().decode('gbk')}")
    except Exception: pass

# ---- 3. code pointers in flash ----
print("\n=== flash pointers (0x08000000..) in data ===")
c = 0
for off in range(0, N-3, 4):
    w = rd32(off)
    if BASE <= w < BASE+N:
        c += 1
print(f"  count={c}")
