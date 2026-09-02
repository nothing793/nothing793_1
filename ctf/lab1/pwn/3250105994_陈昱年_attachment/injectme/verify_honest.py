from capstone import Cs, CS_ARCH_X86, CS_MODE_64
funcs = {
 "ADD": bytes.fromhex("8d0437c3"),
 "SUB": bytes.fromhex("89f829f0c3"),
 "AND": bytes.fromhex("89f821f0c3"),
 "OR":  bytes.fromhex("89f809f0c3"),
 "XOR": bytes.fromhex("89f831f0c3"),
}
md = Cs(CS_ARCH_X86, CS_MODE_64)
for name, b in funcs.items():
    print(f"--- {name} ---")
    for i in md.disasm(b, 0x14000):
        print(f"  0x{i.address:x}: {i.mnemonic} {i.op_str}")
