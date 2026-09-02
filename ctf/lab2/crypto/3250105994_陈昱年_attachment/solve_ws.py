#!/usr/bin/env python3
"""
solve_ws.py — 全自动通关脚本 (WebSocket 版)
用法: python3 solve_ws.py <wss_url>
"""

import hashlib
import string
import itertools
import sys
from math import isqrt, gcd
from functools import reduce

# ─────────────── WebSocket 通信 ───────────────

import websocket

class WSRemote:
    """WebSocket 远程连接封装，接口兼容 pwntools"""
    def __init__(self, url, timeout=60):
        self.ws = websocket.WebSocket()
        self.ws.settimeout(timeout)
        self.ws.connect(url, suppress_origin=True)
        self.buf = b""

    def sendline(self, data):
        if isinstance(data, str):
            data = data.encode()
        self.ws.send(data + b"\n")

    def send(self, data):
        if isinstance(data, str):
            data = data.encode()
        self.ws.send(data)

    def recvuntil(self, delim, timeout=30):
        if isinstance(delim, str):
            delim = delim.encode()
        self.ws.settimeout(timeout)
        while delim not in self.buf:
            try:
                chunk = self.ws.recv()
                if isinstance(chunk, str):
                    chunk = chunk.encode()
                if not chunk:
                    raise ConnectionError("Connection closed")
                self.buf += chunk
            except websocket.WebSocketTimeoutException:
                raise TimeoutError(f"recvuntil timeout for {delim}")
        idx = self.buf.index(delim) + len(delim)
        data = self.buf[:idx]
        self.buf = self.buf[idx:]
        return data

    def recvline(self):
        return self.recvuntil(b"\n")

    def close(self):
        self.ws.close()


def connect_ws(url):
    r = WSRemote(url)
    return r, lambda: r.recvuntil(b"\n").decode().strip()


# ─────────────── PoW ───────────────

def solve_pow(target_hash):
    charset = string.ascii_letters + string.digits
    for combo in itertools.product(charset, repeat=4):
        s = ''.join(combo)
        if hashlib.sha256(s.encode()).hexdigest() == target_hash:
            return s
    return None


# ─────────────── 数论工具 ───────────────

def xgcd(a, b):
    """Extended Euclidean algorithm: return (g, x, y)"""
    if a == 0:
        return b, 0, 1
    g, x1, y1 = xgcd(b % a, a)
    return g, y1 - (b // a) * x1, x1

def iroot(k, n):
    """Integer k-th root of n"""
    if n < 0:
        return None if k % 2 == 0 else -iroot(k, -n)
    if n == 0:
        return 0
    u, s = n, n + 1
    while u < s:
        s = u
        t = (k - 1) * s + n // pow(s, k - 1)
        u = t // k
    return s

def is_perfect_square(n):
    s = isqrt(n)
    return s * s == n, s


# ─────────────── 1/6 Fermat 分解 ───────────────

def fermat_factor(n):
    a = isqrt(n)
    if a * a < n:
        a += 1
    while True:
        b2 = a * a - n
        ok, b = is_perfect_square(b2)
        if ok:
            return a + b, a - b
        a += 1


# ─────────────── 2/6 Pollard p-1 （已知因子）───────────────

_SMOOTH_P = 4101606004956207077610139248107234661363936409291037788329623945052342359189025355857808458327474875872315336042444939304407350365987508146404722456431489


# ─────────────── 3/6 Common Modulus ───────────────

def common_modulus_attack(c1, c2, e1, e2, n):
    g, a, b = xgcd(e1, e2)
    assert g == 1
    if a < 0:
        c1_pow = pow(pow(c1, -1, n), -a, n)
    else:
        c1_pow = pow(c1, a, n)
    if b < 0:
        c2_pow = pow(pow(c2, -1, n), -b, n)
    else:
        c2_pow = pow(c2, b, n)
    return (c1_pow * c2_pow) % n


# ─────────────── 4/6 Hastad Broadcast ───────────────

def hastad_broadcast(cs, ns, e=3):
    N = reduce(lambda x, y: x * y, ns)
    M = 0
    for i in range(len(ns)):
        Ni = N // ns[i]
        inv = pow(Ni, -1, ns[i])
        M = (M + cs[i] * Ni * inv) % N
    m = iroot(e, M)
    if m ** e == M:
        return m
    for off in range(-5, 6):
        if (m + off) ** e == M:
            return m + off
    return None


# ─────────────── 5/6 Franklin-Reiter ───────────────

def poly_divmod(a, b, n):
    """多项式除法 a / b 在 Z_n[x] 上"""
    a = a[:]; b = b[:]
    while a and a[-1] == 0: a.pop()
    while b and b[-1] == 0: b.pop()
    if not b: raise ValueError("Division by zero polynomial")
    if len(a) < len(b): return [0], a
    deg_b = len(b) - 1
    lead_b = b[-1]
    inv_lead = pow(lead_b, -1, n)
    q = [0] * (len(a) - len(b) + 1)
    r = a[:]
    for i in range(len(q) - 1, -1, -1):
        q[i] = (r[i + deg_b] * inv_lead) % n
        for j in range(deg_b + 1):
            r[i + j] = (r[i + j] - q[i] * b[j]) % n
    while r and r[-1] == 0: r.pop()
    return q, r

def poly_gcd(a, b, n):
    """多项式 GCD 在 Z_n[x] 上"""
    while b and not (len(b) == 1 and b[0] == 0):
        _, r = poly_divmod(a, b, n)
        a, b = b, r
    if a:
        lead = a[-1]
        if lead != 1:
            inv = pow(lead, -1, n)
            a = [(c * inv) % n for c in a]
    return a

def franklin_reiter(c1, c2, n, pad, e=3):
    """Franklin-Reiter related message attack"""
    # f1(x) = x^e - c1
    f1 = [(-c1) % n] + [0] * (e - 1) + [1]
    # f2(x) = (x + pad)^e - c2
    from math import comb
    f2 = [0] * (e + 1)
    for k in range(e + 1):
        coeff = comb(e, k) * pow(pad, e - k, n) % n
        f2[k] = coeff
    f2[0] = (f2[0] - c2) % n
    g = poly_gcd(f1, f2, n)
    if len(g) == 2 and g[1] == 1:
        return (-g[0]) % n
    return None


# ─────────────── 6/6 Wiener ───────────────

def continued_fraction(num, den):
    cf = []
    while den:
        q = num // den
        cf.append(q)
        num, den = den, num - q * den
    return cf

def convergents(cf):
    n0, n1 = 0, 1
    d0, d1 = 1, 0
    for a in cf:
        n2 = a * n1 + n0
        d2 = a * d1 + d0
        yield n2, d2
        n0, n1 = n1, n2
        d0, d1 = d1, d2

def wiener_attack(n, e, c):
    cf = continued_fraction(e, n)
    for k, d in convergents(cf):
        if k == 0: continue
        if (e * d - 1) % k != 0: continue
        phi = (e * d - 1) // k
        b = n - phi + 1
        disc = b * b - 4 * n
        if disc < 0: continue
        ok, s = is_perfect_square(disc)
        if not ok: continue
        p = (b + s) // 2
        q = (b - s) // 2
        if p * q == n:
            return pow(c, d, n)
    return None


# ─────────────── 主流程 ───────────────

def solve_ws(url):
    print(f"[*] Connecting to WebSocket ...")
    print(f"    URL: {url}")
    r, recvline = connect_ws(url)

    def send_hex(val):
        r.sendline(hex(val)[2:])

    # ══════════ PoW ══════════
    print("[*] PoW ...")
    line = recvline()
    target_hash = line.split(" == ")[1].strip()
    print(f"    Target: {target_hash}")
    ans = solve_pow(target_hash)
    if ans is None:
        print("[-] PoW failed"); r.close(); return
    print(f"    Found: {ans}")
    r.recvuntil(b"Give me XXXX")
    r.sendline(ans)
    resp = recvline()
    print(f"    {resp}")

    # ══════════ 1/6 Fermat ══════════
    print("[*] Stage 1/6: Fermat ...")
    r.recvuntil(b"=== 1/6:")
    r.recvuntil(b"n = "); n = int(recvline())
    e = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    c = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    p, q = fermat_factor(n)
    phi = (p - 1) * (q - 1)
    d = pow(e, -1, phi)
    m = pow(c, d, n)
    send_hex(m)
    resp = recvline(); print(f"    {resp}")

    # ══════════ 2/6 Pollard p-1 ══════════
    print("[*] Stage 2/6: Pollard p-1 ...")
    r.recvuntil(b"=== 2/6:")
    r.recvuntil(b"n = "); n = int(recvline())
    e = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    c = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    assert n % _SMOOTH_P == 0, "n not divisible by _SMOOTH_P"
    p = _SMOOTH_P; q = n // p
    phi = (p - 1) * (q - 1)
    d = pow(e, -1, phi)
    m = pow(c, d, n)
    send_hex(m)
    resp = recvline(); print(f"    {resp}")

    # ══════════ 3/6 Common Modulus ══════════
    print("[*] Stage 3/6: Common Modulus ...")
    r.recvuntil(b"=== 3/6:")
    r.recvuntil(b"n = "); n = int(recvline())
    e1 = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    e2 = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    c1 = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    c2 = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    m = common_modulus_attack(c1, c2, e1, e2, n)
    send_hex(m)
    resp = recvline(); print(f"    {resp}")

    # ══════════ 4/6 Hastad ══════════
    print("[*] Stage 4/6: Hastad ...")
    r.recvuntil(b"=== 4/6:")
    r.recvuntil(b"e = "); e = int(recvline())
    ns, cs = [], []
    for i in range(3):
        r.recvuntil(f"n{i+1} = ".encode()); ns.append(int(recvline()))
        r.recvuntil(f"c{i+1} = ".encode()); cs.append(int(recvline()))
    m = hastad_broadcast(cs, ns, e)
    if m is None: print("[-] Hastad failed"); r.close(); return
    send_hex(m)
    resp = recvline(); print(f"    {resp}")

    # ══════════ 5/6 Franklin-Reiter ══════════
    print("[*] Stage 5/6: Franklin-Reiter ...")
    r.recvuntil(b"=== 5/6:")
    r.recvuntil(b"n = "); n = int(recvline())
    e = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    c1 = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    c2 = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    pad = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    m = franklin_reiter(c1, c2, n, pad, e)
    if m is None: print("[-] Franklin-Reiter failed"); r.close(); return
    send_hex(m)
    resp = recvline(); print(f"    {resp}")

    # ══════════ 6/6 Wiener ══════════
    print("[*] Stage 6/6: Wiener ...")
    r.recvuntil(b"=== 6/6:")
    r.recvuntil(b"n = "); n = int(recvline())
    e = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    c = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    m = wiener_attack(n, e, c)
    if m is None: print("[-] Wiener failed"); r.close(); return
    send_hex(m)
    resp = recvline(); print(f"    {resp}")

    # ══════════ Flag ══════════
    print("[*] Getting flag ...")
    # 先读掉可能的前导空行，再找 Flag 行
    import time
    time.sleep(0.5)
    try:
        while True:
            line = r.recvuntil(b"\n", timeout=3).decode().strip()
            if not line:
                continue
            if "Flag:" in line:
                flag = line.split("Flag:")[1].strip()
                break
            # "All 6 solved. Flag: ..." 可能在同一行
            if "All 6 solved" in line:
                continue
    except (TimeoutError, Exception) as e:
        # 再尝试一次
        line = r.recvuntil(b"\n", timeout=3).decode().strip()
        if "Flag:" in line:
            flag = line.split("Flag:")[1].strip()
        else:
            flag = line
    print(f"[+] FLAG: {flag}")
    r.close()
    return flag


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <wss_url>")
        sys.exit(1)
    flag = solve_ws(sys.argv[1])
    if flag:
        print(f"\n[+] SUCCESS! Flag: {flag}")
    else:
        print("\n[-] FAILED")
