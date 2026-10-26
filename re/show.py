import struct, sys
from capstone import *
from capstone.arm import *

BASE=0x08000000
img=open('fw.bin','rb').read(); N=len(img)
md=Cs(CS_ARCH_ARM, CS_MODE_THUMB+CS_MODE_MCLASS); md.detail=True

def u32(a):
    o=a-BASE; return struct.unpack_from('<I',img,o)[0] if 0<=o<=N-4 else None
def u16(a):
    o=a-BASE; return struct.unpack_from('<H',img,o)[0] if 0<=o<=N-2 else None
def cstr(a):
    o=a-BASE
    if not (0<=o<N): return None
    e=img.find(b'\x00',o)
    if e<0 or e-o>80: return None
    s=img[o:e]
    if len(s)<2 or not all(32<=c<127 for c in s): return None
    return s.decode()

PERI={0x40010000:'DBGMCU',0x40010800:'GPIOA',0x40010C00:'GPIOB',0x40011000:'GPIOC',
0x40011400:'GPIOD',0x40011800:'GPIOE',0x40012400:'AFIO',0x40012800:'EXTI',0x40012C00:'TIM1',
0x40013000:'SPI1',0x40013400:'TIM8',0x40013800:'USART1',0x40013C00:'ADC1',0x40014000:'ADC2',
0x40000000:'TIM2',0x40000400:'TIM3',0x40000800:'TIM4',0x40000C00:'TIM5',0x40001000:'TIM6',
0x40001400:'TIM7',0x40003000:'I2C1',0x40003400:'I2C2',0x40003800:'SPI2',0x40004400:'USART2',
0x40004800:'USART3',0x40020000:'DMA1',0x40021000:'RCC',0x40022000:'FLASH',0xE000E010:'SysTick',
0xE000E100:'NVIC',0xE000ED00:'SCB',0xE000ED08:'VTOR',0xE000ED88:'CPACR',0xE0042000:'DBGMCU'}
def per(a):
    for b,n in PERI.items():
        if a==b: return n
        if b<a<b+0x40: return f'{n}+0x{a-b:02X}'
    if 0x20000000<=a<0x20008000: return 'RAM'
    return ''

def show(lo,hi):
    a=lo
    while a<hi:
        got=list(md.disasm(img[a-BASE:min(a-BASE+4,N)],a,1))
        if not got or got[0].address!=a:
            print(f"  {a:08x}: {img[a-BASE:a-BASE+2].hex():<9} .hword    0x{u16(a):04x}"); a+=2; continue
        i=got[0]; s=f"  {a:08x}: {img[a-BASE:a-BASE+i.size].hex():<9} {i.mnemonic:<8} {i.op_str}"
        extra=''
        if i.mnemonic=='adr' and i.operands:
            imm=next((o.imm for o in i.operands if o.type==ARM_OP_IMM), None)
            if imm is not None:
                v=(((a+4)&~3)+imm)&0xFFFFFFFF
                st=cstr(v)
                extra+=f"  ; =0x{v:08x}"
                if st: extra+=f' "{st}"'
        for op in i.operands:
            if op.type==ARM_OP_MEM and op.mem.base==ARM_REG_PC:
                lit=((a+4)&~3)+op.mem.disp
                v=u32(lit)
                if v is not None:
                    st=cstr(v); p=per(v)
                    extra+=f"  ; =0x{v:08x}"
                    if p: extra+=f" {p}"
                    if st: extra+=f' "{st}"'
                    elif v and BASE<=v<BASE+N: extra+=f" code:{v&~1:08x}"
            elif op.type==ARM_OP_IMM:
                if per(op.imm): extra+=f"  ; {per(op.imm)}"
        print(s+extra)
        a+=i.size

if __name__=='__main__':
    lo=int(sys.argv[1],16); hi=int(sys.argv[2],16)
    show(lo,hi)
