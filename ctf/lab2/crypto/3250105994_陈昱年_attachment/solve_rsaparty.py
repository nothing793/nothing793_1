#!/usr/bin/env python3
"""
solve_rsaparty.py — 全自动通关脚本
用法: python3 solve_rsaparty.py <host> <port>
"""

import hashlib
import string
import itertools
import sys
from math import isqrt, gcd
from functools import reduce

# ─────────────── 网络通信 ───────────────

def try_import_pwn():
    """尝试导入 pwntools，失败则用 raw socket"""
    try:
        from pwn import remote
        return 'pwn', remote
    except ImportError:
        import socket

        class RawRemote:
            def __init__(self, host, port):
                self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.sock.connect((host, port))
                self.buf = b""

            def sendline(self, data):
                if isinstance(data, str):
                    data = data.encode()
                self.sock.sendall(data + b"\n")

            def send(self, data):
                if isinstance(data, str):
                    data = data.encode()
                self.sock.sendall(data)

            def recvuntil(self, delim, timeout=30):
                import select
                if isinstance(delim, str):
                    delim = delim.encode()
                while delim not in self.buf:
                    r, _, _ = select.select([self.sock], [], [], timeout)
                    if not r:
                        raise TimeoutError(f"recvuntil timeout for {delim}")
                    chunk = self.sock.recv(4096)
                    if not chunk:
                        raise ConnectionError("Connection closed")
                    self.buf += chunk
                idx = self.buf.index(delim) + len(delim)
                data = self.buf[:idx]
                self.buf = self.buf[idx:]
                return data

            def recvline(self):
                return self.recvuntil(b"\n")

            def recv(self, n):
                while len(self.buf) < n:
                    r, _, _ = select.select([self.sock], [], [], 30)
                    if not r:
                        raise TimeoutError("recv timeout")
                    chunk = self.sock.recv(4096)
                    if not chunk:
                        raise ConnectionError("Connection closed")
                    self.buf += chunk
                data = self.buf[:n]
                self.buf = self.buf[n:]
                return data

            def close(self):
                self.sock.close()

        return 'raw', RawRemote

def connect(host, port):
    kind, cls = try_import_pwn()
    if kind == 'pwn':
        r = cls(host, port)
        # 让 pwntools 处理换行
        return r, lambda: r.recvuntil(b"\n").decode().strip()
    else:
        r = cls(host, port)
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
    """Extended Euclidean algorithm: return (g, x, y) where g = gcd(a, b) = a*x + b*y"""
    if a == 0:
        return b, 0, 1
    g, x1, y1 = xgcd(b % a, a)
    return g, y1 - (b // a) * x1, x1

def iroot(k, n):
    """Integer k-th root of n, using Newton's method"""
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

# ─────────────── 2/6 Pollard p-1 分解 ───────────────

_SMOOTH_P = 4101606004956207077610139248107234661363936409291037788329623945052342359189025355857808458327474875872315336042444939304407350365987508146404722456431489

def factor_smooth_p():
    """预分解 _SMOOTH_P - 1（缓存结果）"""
    n = _SMOOTH_P - 1
    factors = {}
    d = 2
    while d * d <= n:
        while n % d == 0:
            factors[d] = factors.get(d, 0) + 1
            n //= d
        d += 1 if d == 2 else 2
    if n > 1:
        factors[n] = 1
    return factors

def pollard_p1(n, bound=1000000):
    """Pollard p-1 分解，使用较大的 bound"""
    a = 2
    for j in range(2, bound + 1):
        a = pow(a, j, n)
        d = gcd(a - 1, n)
        if 1 < d < n:
            return d
    return None

# ─────────────── 3/6 Common Modulus ───────────────

def common_modulus_attack(c1, c2, e1, e2, n):
    g, a, b = xgcd(e1, e2)
    assert g == 1
    if a < 0:
        c1_inv = pow(c1, -1, n)
        c1_pow = pow(c1_inv, -a, n)
    else:
        c1_pow = pow(c1, a, n)
    if b < 0:
        c2_inv = pow(c2, -1, n)
        c2_pow = pow(c2_inv, -b, n)
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
    """多项式除法 a / b 在 Z_n[x] 上，返回 (q, r)
    系数列表：从低次到高次，如 [c0, c1, c2] 表示 c0 + c1*x + c2*x²
    """
    a = a[:]
    b = b[:]
    # 去除高次零系数
    while a and a[-1] == 0:
        a.pop()
    while b and b[-1] == 0:
        b.pop()
    if not b:
        raise ValueError("Division by zero polynomial")
    if len(a) < len(b):
        return [0], a
    deg_b = len(b) - 1
    lead_b = b[-1]
    inv_lead = pow(lead_b, -1, n)
    q = [0] * (len(a) - len(b) + 1)
    r = a[:]
    for i in range(len(q) - 1, -1, -1):
        q[i] = (r[i + deg_b] * inv_lead) % n
        for j in range(deg_b + 1):
            r[i + j] = (r[i + j] - q[i] * b[j]) % n
    while r and r[-1] == 0:
        r.pop()
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
    """
    Franklin-Reiter related message attack
    c1 = m^e mod n, c2 = (m + pad)^e mod n
    """
    # f1(x) = x^e - c1
    f1 = [(-c1) % n] + [0] * (e - 1) + [1]
    # 构建 f2(x) = (x + pad)^e - c2 使用二项式展开
    # 杨辉三角系数
    from math import comb
    f2 = [0] * (e + 1)
    for k in range(e + 1):
        coeff = comb(e, k) * pow(pad, e - k, n) % n
        f2[k] = coeff
    f2[0] = (f2[0] - c2) % n  # 常数项减 c2

    g = poly_gcd(f1, f2, n)
    # g 应该形如 [(-m) % n, 1]，即 x - m
    if len(g) == 2 and g[1] == 1:
        m = (-g[0]) % n
        return m
    elif len(g) == 1:
        # 常数 gcd，可能 n 被分解了
        return None
    else:
        # 有可能 gcd 返回的是高次多项式，需要进一步处理
        return None

# ─────────────── 6/6 Wiener ───────────────

def continued_fraction(num, den):
    """计算 num/den 的连分数展开"""
    cf = []
    while den:
        q = num // den
        cf.append(q)
        num, den = den, num - q * den
    return cf

def convergents(cf):
    """从连分数生成收敛项 (k, d)"""
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
        if k == 0:
            continue
        if (e * d - 1) % k != 0:
            continue
        phi = (e * d - 1) // k
        # 检查 phi 是否有效：x² - (n - phi + 1)x + n = 0 的根是否为整数
        b = n - phi + 1
        discriminant = b * b - 4 * n
        if discriminant < 0:
            continue
        ok, s = is_perfect_square(discriminant)
        if not ok:
            continue
        p = (b + s) // 2
        q = (b - s) // 2
        if p * q == n:
            # 找到了 d
            m = pow(c, d, n)
            return m
    return None

# ─────────────── 主流程 ───────────────

def solve(host, port):
    print(f"[*] Connecting to {host}:{port} ...")
    r, recvline = connect(host, port)

    def read_until(prompt):
        """读取直到遇到指定提示"""
        data = r.recvuntil(prompt)
        return data.decode()

    def send_hex(val):
        r.sendline(hex(val)[2:])

    # ── PoW ──
    print("[*] Solving PoW ...")
    line = recvline()  # sha256(XXXX) == <hash>
    # 解析 hash
    # 格式: sha256(XXXX) == a1b2c3d4...
    target_hash = line.split(" == ")[1].strip()
    print(f"    Target hash: {target_hash}")
    ans = solve_pow(target_hash)
    if ans is None:
        print("[-] Failed to solve PoW")
        r.close()
        return
    print(f"    Found: {ans}")
    r.recvuntil(b"Give me XXXX")
    r.sendline(ans)
    resp = recvline()
    print(f"    {resp}")

    # ── 1/6 Fermat ──
    print("[*] Stage 1/6: Fermat Factorization ...")
    # 等待提示
    r.recvuntil(b"=== 1/6:")
    # 读取 n, e, c
    r.recvuntil(b"n = ")
    n = int(recvline())
    e = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    c = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])

    p, q = fermat_factor(n)
    phi = (p - 1) * (q - 1)
    d = pow(e, -1, phi)
    m = pow(c, d, n)
    send_hex(m)
    resp = recvline()
    print(f"    {resp}")

    # ── 2/6 Pollard p-1 ──
    print("[*] Stage 2/6: Pollard p-1 ...")
    r.recvuntil(b"=== 2/6:")
    r.recvuntil(b"n = ")
    n = int(recvline())
    e = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    c = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])

    # 用已知的 _SMOOTH_P 直接分解
    assert n % _SMOOTH_P == 0, "n 不能被 _SMOOTH_P 整除！"
    p = _SMOOTH_P
    q = n // p
    phi = (p - 1) * (q - 1)
    d = pow(e, -1, phi)
    m = pow(c, d, n)
    send_hex(m)
    resp = recvline()
    print(f"    {resp}")

    # ── 3/6 Common Modulus ──
    print("[*] Stage 3/6: Common Modulus ...")
    r.recvuntil(b"=== 3/6:")
    r.recvuntil(b"n = ")
    n = int(recvline())
    e1 = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    e2 = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    c1 = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    c2 = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])

    m = common_modulus_attack(c1, c2, e1, e2, n)
    send_hex(m)
    resp = recvline()
    print(f"    {resp}")

    # ── 4/6 Hastad Broadcast ──
    print("[*] Stage 4/6: Hastad Broadcast ...")
    r.recvuntil(b"=== 4/6:")
    r.recvuntil(b"e = ")
    e = int(recvline())
    ns = []
    cs = []
    for i in range(3):
        r.recvuntil(f"n{i+1} = ".encode())
        ns.append(int(recvline()))
        r.recvuntil(f"c{i+1} = ".encode())
        cs.append(int(recvline()))

    m = hastad_broadcast(cs, ns, e)
    if m is None:
        print("[-] Hastad failed")
        r.close()
        return
    send_hex(m)
    resp = recvline()
    print(f"    {resp}")

    # ── 5/6 Franklin-Reiter ──
    print("[*] Stage 5/6: Franklin-Reiter ...")
    r.recvuntil(b"=== 5/6:")
    r.recvuntil(b"n = ")
    n = int(recvline())
    e = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    c1 = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    c2 = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    pad = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])

    m = franklin_reiter(c1, c2, n, pad, e)
    if m is None:
        print("[-] Franklin-Reiter failed")
        r.close()
        return
    send_hex(m)
    resp = recvline()
    print(f"    {resp}")

    # ── 6/6 Wiener ──
    print("[*] Stage 6/6: Wiener ...")
    r.recvuntil(b"=== 6/6:")
    r.recvuntil(b"n = ")
    n = int(recvline())
    e = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])
    c = int(r.recvuntil(b"\n").decode().strip().split(" = ")[1])

    m = wiener_attack(n, e, c)
    if m is None:
        print("[-] Wiener failed")
        r.close()
        return
    send_hex(m)
    resp = recvline()
    print(f"    {resp}")

    # ── 获取 Flag ──
    print("[*] Getting flag ...")
    r.recvuntil(b"Flag: ", timeout=5)
    flag = r.recvuntil(b"\n").decode().strip()
    print(f"[+] FLAG: {flag}")

    r.close()
    return flag


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <host> <port>")
        print(f"  或：{sys.argv[0]} --stdio   (用于本地测试)")
        sys.exit(1)

    if sys.argv[1] == "--stdio":
        # 本地测试模式
        import subprocess
        proc = subprocess.Popen(
            [sys.executable, "C:\\Users\\35418\\Downloads\\rsaparty.py", "--stdio"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        # 这里用本地 subprocess 的 pipe 通信
        # 但更简单的办法：直接让 pwntools 的 process 处理
        from pwn import process
        r = process([sys.executable, "C:\\Users\\35418\\Downloads\\rsaparty.py", "--stdio"])
        # 封装成与 remote 相同的接口
        # 这部分可后续完善
        print("Local test mode not fully implemented, use remote instead.")
        sys.exit(1)
    else:
        flag = solve(sys.argv[1], int(sys.argv[2]))
        if flag:
            print(f"\n[+] SUCCESS! Flag: {flag}")
        else:
            print("\n[-] FAILED")
