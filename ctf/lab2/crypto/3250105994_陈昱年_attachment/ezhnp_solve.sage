from Crypto.Util.number import bytes_to_long, long_to_bytes
from hashlib import sha256
from sage.all import *
import re

# ========== 曲线参数 ==========
p = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F
a = 0
b = 7
n = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141
Gx = 0x79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798
Gy = 0x483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8

msg = b"message_to_sign"
h = bytes_to_long(sha256(msg).digest())

# ========== 从 output.txt 读取 R 和 S ==========
with open("ezhnp_output.txt", "r") as f:
    data = f.read()

# 使用正则提取列表（支持换行和空格）
r_match = re.search(r'R\s*=\s*(\[.*?\])', data, re.DOTALL)
s_match = re.search(r'S\s*=\s*(\[.*?\])', data, re.DOTALL)

if not r_match or not s_match:
    raise ValueError("无法从 output.txt 中解析 R 或 S 列表")

R = eval(r_match.group(1))
S = eval(s_match.group(1))

t = len(R)
assert t == len(S), "R 和 S 长度不一致"

# ========== 计算 A_i, B_i ==========
A = []
B = []
for r, s in zip(R, S):
    s_inv = inverse_mod(s, n)
    A.append((r * s_inv) % n)
    B.append((h * s_inv) % n)

# ========== 构造格 ==========
D = 2^16   # 缩放因子
M = matrix(ZZ, t + 1, t + 1)
for i in range(t):
    M[i, i] = n * D
for j in range(t):
    M[t, j] = A[j] * D
M[t, t] = 1

target = vector(ZZ, [-B[i] * D for i in range(t)] + [0])

# ========== Babai 最近平面算法 ==========
def babai(B, target):
    B = B.LLL()
    G = B.gram_schmidt()[0]
    b = target
    for i in reversed(range(B.nrows())):
        c = round((b * G[i]) / (G[i] * G[i]))
        b -= c * B[i]
    return target - b

error = babai(M, target)
sk = error[-1] % n
flag = long_to_bytes(sk)

print("Flag:", flag.decode())