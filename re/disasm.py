import struct, sys, collections, json, os
from pathlib import Path
from capstone import *
from capstone.arm import *

HERE = os.path.realpath(os.path.dirname(os.path.abspath(__file__)))

def local(name):
    """Resolve a bare filename inside this script's directory and prove containment."""
    if os.path.basename(name) != name or name in ('', '.'):
        raise ValueError(f"unsafe filename: {name!r}")
    path = os.path.realpath(os.path.join(HERE, name))
    if os.path.commonpath([path, HERE]) != HERE:
        raise ValueError(f"path escapes analysis directory: {name!r}")
    return path

BASE = 0x08000000
img = open(local('fw.bin'),'rb').read()
N = len(img)
LO, HI = BASE, BASE + N
RAM_LO, RAM_HI = 0x20000000, 0x20008000

md = Cs(CS_ARCH_ARM, CS_MODE_THUMB + CS_MODE_MCLASS)
md.detail = True
md.skipdata = False

def u32(a):
    o = a - BASE
    if 0 <= o <= N-4: return struct.unpack_from('<I', img, o)[0]
    return None
def u16(a):
    o = a - BASE
    if 0 <= o <= N-2: return struct.unpack_from('<H', img, o)[0]
    return None
def is_code(a): return LO <= a < HI

F1 = {
 0x40010000:'DBGMCU',0x40010800:'GPIOA',0x40010C00:'GPIOB',0x40011000:'GPIOC',
 0x40011400:'GPIOD',0x40011800:'GPIOE',0x40011C00:'GPIOF',0x40012000:'GPIOG',
 0x40012400:'AFIO',0x40012800:'EXTI',0x40012C00:'TIM1',0x40013000:'SPI1',
 0x40013400:'TIM8',0x40013800:'USART1',0x40013C00:'ADC1',0x40014000:'ADC2',
 0x40000000:'TIM2',0x40000400:'TIM3',0x40000800:'TIM4',0x40000C00:'TIM5',
 0x40001000:'TIM6',0x40001400:'TIM7',0x40001C00:'SPI3',0x40002000:'I2S3',
 0x40003000:'I2C1',0x40003400:'I2C2',0x40003800:'SPI2',0x40003C00:'USB',
 0x40004400:'USART2',0x40004800:'USART3',0x40004C00:'UART4',0x40005000:'UART5',
 0x40005400:'I2C1',0x40005800:'I2C2',0x40005C00:'USB/CAN',0x40006000:'CAN',
 0x40006400:'BKP',0x40006800:'PWR',0x40006C00:'DAC',0x40007000:'ADC1',
 0x40007400:'ADC2',0x40007800:'ADC3',0x40007C00:'ADC_common',
 0x40020000:'DMA1',0x40020400:'DMA2',0x40021000:'RCC',0x40022000:'FLASH',
 0x40023000:'CRC',0xE000E010:'SysTick',0xE000E100:'NVIC',
 0xE000ED00:'SCB',0xE000ED08:'SCB_VTOR',0xE000ED88:'SCB_CPACR',
 0xE0042000:'DBGMCU',0xE000E004:'SCB_ICTR',0xE000ED04:'SCB_ICSR',
}
def lbl(a):
    if a is None: return '?'
    if a in F1: return F1[a]
    for b,n in F1.items():
        if b < a < b+0x400 and (a-b) < 0x40: return f'{n}+0x{a-b:X}'
    if RAM_LO <= a < RAM_HI: return 'RAM'
    return ''

def cstr(a, maxlen=60):
    o = a - BASE
    if not (0 <= o < N): return None
    end = img.find(b'\x00', o)
    if end < 0: return None
    s = img[o:end]
    if len(s) < 3 or not s.isascii(): return None
    if not all(32 <= c < 127 for c in s): return None
    return s.decode()

# ---------- pass 1: entry points ----------
entries = set()
for i in range(1, 96):
    w = u32(BASE + i*4)
    if w and is_code(w & ~1):
        entries.add(w & ~1)
print("entries from vector table:", [hex(x) for x in sorted(entries)])

# ---------- recursive descent ----------
visited = set()
hits = collections.Counter()
call_targets = set(entries)

def decode_at(a):
    o = a - BASE
    chunk = img[o:o+4]
    ins = list(md.disasm(chunk, a, 1))
    if not ins: return None
    return ins[0]

def walk(start):
    work = [start]
    while work:
        a = work.pop()
        while True:
            if a in visited or not is_code(a) or a >= HI:
                break
            ins = decode_at(a)
            if ins is None or ins.address != a:
                break
            visited.add(a)
            hits[a] += 1
            m = ins.mnemonic
            stop = False
            fallthrough = True
            groups = set(ins.groups)
            if CS_GRP_JUMP in groups and ins.operands:
                op = ins.operands[0]
                if op.type == ARM_OP_IMM:
                    tgt = op.imm
                    if m == 'b':
                        fallthrough = False
                    if is_code(tgt):
                        work.append(tgt)
                    else:
                        stop = True
                elif op.type == ARM_OP_REG:
                    if m == 'bx':
                        stop = True
                else:
                    stop = True
                    fallthrough = False
            elif CS_GRP_CALL in groups and ins.operands:
                op = ins.operands[0]
                if op.type == ARM_OP_IMM:
                    tgt = op.imm
                    if is_code(tgt):
                        call_targets.add(tgt)
                        work.append(tgt)
            elif CS_GRP_RET in groups:
                stop = True
                fallthrough = False
            elif m == 'pop' and ins.op_str and 'pc' in ins.op_str:
                stop = True
                fallthrough = False
            elif m == 'ldr' and ins.op_str.startswith('pc,'):
                stop = True
                fallthrough = False
            elif m in ('tbb','tbh'):
                stop = True
                fallthrough = False
            elif m.startswith('udf') or m == 'bkpt':
                stop = True
                fallthrough = False
            if stop or not fallthrough:
                break
            a = ins.address + ins.size

queue = sorted(entries)
done_entries = set()
while queue:
    s = queue.pop()
    if s in done_entries: continue
    done_entries.add(s)
    walk(s)
while True:
    rem = [t for t in call_targets if t not in done_entries]
    if not rem: break
    for t in rem:
        if t in done_entries: continue
        done_entries.add(t); walk(t)

# ---------- pass 2: find missed push {...,lr} prologues ----------
missed = []
for a in range(LO+0x148, HI-2, 2):
    if a in visited: continue
    hw = u16(a)
    if hw is None: continue
    if (hw & 0xFF00) == 0xB500:
        missed.append(a)
print(f"unvisited push+lr candidates: {len(missed)} -> {[hex(x) for x in missed[:40]]}")
for a in missed:
    if a in done_entries: continue
    done_entries.add(a)
    call_targets.add(a)
    walk(a)
    while True:
        rem = [t for t in call_targets if t not in done_entries]
        if not rem: break
        for t in rem:
            if t in done_entries: continue
            done_entries.add(t); walk(t)

print(f"visited insns={len(visited)} entries={len(done_entries)}")

starts = sorted(done_entries)
func_ranges = {}
for i, s in enumerate(starts):
    nxt = starts[i+1] if i+1 < len(starts) else HI
    func_ranges[s] = nxt

out = []
def fmt_ins(a, ins):
    b = img[a-BASE:a-BASE+ins.size].hex()
    line = f"  {a:08x}: {b:<10} {ins.mnemonic:<8} {ins.op_str}"
    extra = ''
    for op in ins.operands:
        if op.type == ARM_OP_MEM and op.mem.base == ARM_REG_PC:
            lit = (a + 4) & ~3
            lit += op.mem.disp
            v = u32(lit)
            if v is not None:
                s = cstr(v)
                l = lbl(v)
                extra += f"  ; [pc]@{lit:x} = 0x{v:08x}"
                if l: extra += f" {l}"
                if s: extra += f' "{s}"'
                elif is_code(v & ~1): extra += f" code:{v&~1:08x}"
        if op.type == ARM_OP_IMM:
            v = op.imm
            if 0x40000000 <= v < 0x50000000 or 0xE0000000 <= v < 0xE000F000:
                l = lbl(v)
                if l: extra += f"  ; {l}"
            if is_code(v) and ins.mnemonic in ('bl','blx'):
                extra += f"  ; fn_{v:08x}"
    return line + extra

for s in starts:
    end = func_ranges[s]
    out.append(f"\n; ===== FUNC_{s:08x}  (0x{end-s:X} bytes) =====")
    a = s
    while a < end and a < HI:
        ins = decode_at(a)
        if ins is None:
            if u16(a) is not None:
                out.append(f"  {a:08x}: {img[a-BASE:a-BASE+2].hex():<10} .hword    0x{u16(a):04x}")
            a += 2
            continue
        if a in visited:
            out.append(fmt_ins(a, ins))
        else:
            if ins.size == 4:
                out.append(f"  {a:08x}: {img[a-BASE:a-BASE+4].hex():<10} .word     0x{u32(a):08x}")
            else:
                out.append(f"  {a:08x}: {img[a-BASE:a-BASE+2].hex():<10} .hword    0x{u16(a):04x}")
        a += ins.size

Path(local('listing.txt')).write_text('\n'.join(out), encoding='utf-8')

rows = []
callers = collections.defaultdict(set)
a = LO
while a < HI-3:
    if a in visited:
        ins = decode_at(a)
        if ins and ins.mnemonic in ('bl','blx') and ins.operands and ins.operands[0].type == ARM_OP_IMM:
            callers[ins.operands[0].imm & ~1].add(a)
            a += ins.size; continue
        if ins:
            a += ins.size; continue
    a += 2
for s in starts:
    rows.append({'addr': s, 'size': func_ranges[s]-s, 'callers': len(callers.get(s,()))})
Path(local('functions.json')).write_text(json.dumps(rows, indent=1), encoding='utf-8')
print(f"functions={len(starts)}")
print("largest:")
for r in sorted(rows, key=lambda x:-x['size'])[:30]:
    print(f"  FUNC_{r['addr']:08x} size=0x{r['size']:X} callers={r['callers']}")
