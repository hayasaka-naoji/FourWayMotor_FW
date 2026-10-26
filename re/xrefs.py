import struct, collections
from capstone import *
from capstone.arm import *

BASE = 0x08000000
img = open('fw.bin','rb').read(); N = len(img); LO, HI = BASE, BASE+N
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB + CS_MODE_MCLASS); md.detail = True

def u32(a):
    o=a-BASE
    return struct.unpack_from('<I',img,o)[0] if 0<=o<=N-4 else None
def cstr(a):
    o=a-BASE
    if not (0<=o<N): return None
    e=img.find(b'\x00',o)
    if e<0: return None
    s=img[o:e]
    if len(s)<3 or not s.isascii() or not all(32<=c<127 for c in s): return None
    return s.decode()

# decode whole image linearly (for xref scan only)
insns=[]
a=LO
while a < HI-1:
    got=list(md.disasm(img[a-BASE:a-BASE+4], a, 1))
    if got and got[0].address==a:
        insns.append(got[0]); a+=got[0].size
    else:
        a+=2
print(f"linear decode: {len(insns)} insns")

# string xrefs
print("\n=== string references ===")
by_str=collections.defaultdict(list)
for ins in insns:
    for op in ins.operands:
        if op.type==ARM_OP_MEM and op.mem.base==ARM_REG_PC:
            lit=((ins.address+4)&~3)+op.mem.disp
            v=u32(lit)
            if v is not None:
                s=cstr(v)
                if s: by_str[s].append(ins.address)
                elif v in (0x08001827,): pass
for s, refs in sorted(by_str.items(), key=lambda x: min(x[1])):
    print(f"  {s:<30} refs: {', '.join(f'{r:08x}' for r in refs)}")

# address-taken flash pointers (literal pools holding code addresses) -> function pointer tables
print("\n=== literal pools holding code pointers ===")
for off in range(0, N-3, 4):
    v=u32(BASE+off)
    if v and LO <= v < HI:
        # find who loads it
        refs=[ins.address for ins in insns for op in ins.operands
              if op.type==ARM_OP_MEM and op.mem.base==ARM_REG_PC
              and ((ins.address+4)&~3)+op.mem.disp==BASE+off]
        print(f"  pool@{BASE+off:08x} -> code {v&~1:08x}  loaded_by={['%08x'%r for r in refs][:6]}")
