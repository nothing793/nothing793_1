#!/usr/bin/env python3
"""
regev_solve — 使用 fpylll 的 LLL + CVP.closest_vector
"""
import sys, time, random
from hashlib import sha256
from Crypto.Cipher import AES
from Crypto.Util.Padding import unpad

# 自动定位到同目录下的 regev_output.py
import os
script_dir = os.path.dirname(os.path.abspath(__file__))
data_path = os.path.join(script_dir, "regev_output.py")
exec(open(data_path).read())
print(f"参数: n={n}, m={m}, q={q}")

from fpylll import IntegerMatrix, LLL, CVP
import numpy as np

dim = m + n
print(f"[1] 构造 {dim}x{dim} 格基...")
M = IntegerMatrix(dim, dim)
for i in range(m):
    M[i, i] = q
for j in range(m):
    for i in range(n):
        M[m + i, j] = A[j][i]
for i in range(n):
    M[m + i, m + i] = 1

target = [b[i] for i in range(m)] + [0] * n

print("[2] LLL 约简...")
start = time.time()
M_lll = LLL.reduction(M)
print(f"    LLL 耗时: {time.time()-start:.2f} 秒")

print("[3] CVP.closest_vector...")
start = time.time()
# fpylll 的 closest_vector 用 Babai nearest plane + 枚举优化
w = CVP.closest_vector(M_lll, target)
print(f"    CVP 耗时: {time.time()-start:.2f} 秒")

s = [w[m + i] for i in range(n)]
print(f"    s前10位: {s[:10]}")

A_np = np.array(A, dtype=np.int64)
b_np = np.array(b, dtype=np.int64)

def compute_errs(s):
    bc = (A_np @ np.array(s, dtype=np.int64)) % q
    diff = [int(bc[i]) - int(b_np[i]) for i in range(m)]
    diff = [d if d <= q // 2 else d - q for d in diff]
    return sum(1 for d in diff if d not in (-1, 0, 1)), diff

errs, diff = compute_errs(s)
print(f"    误差数: {errs}/{m}")

def refine(s, diff, max_iter=2000):
    errs = sum(1 for d in diff if d not in (-1, 0, 1))
    improved = True
    iters = 0
    while improved and iters < max_iter and errs > 0:
        improved = False
        iters += 1
        for i in range(n):
            old = s[i]
            for new_val in (0, 1):
                if new_val == old:
                    continue
                s[i] = new_val
                delta = new_val - old
                col = A_np[:, i]
                for j in range(m):
                    diff[j] = (diff[j] + int(delta * col[j])) % q
                    if diff[j] > q // 2:
                        diff[j] -= q
                new_errs = sum(1 for d in diff if d not in (-1, 0, 1))
                if new_errs < errs:
                    errs = new_errs
                    improved = True
                    break
                else:
                    s[i] = old
                    for j in range(m):
                        diff[j] = (diff[j] - int(delta * col[j])) % q
                        if diff[j] > q // 2:
                            diff[j] -= q
    return s, errs

if errs == 0:
    print("[✓] 成功!")
else:
    print(f"[!] 误差 {errs}，贪心修正...")
    s, errs = refine(s, diff)
    print(f"    修正后: {errs}/{m}")
    
    if errs > 0:
        print("[!] 随机扰动重启...")
        found = False
        for trial in range(500):
            s_rand = s[:]
            k = random.randint(5, 15)
            for idx in random.sample(range(n), k):
                s_rand[idx] = 1 - s_rand[idx]
            
            s_try = s_rand[:]
            bc = (A_np @ np.array(s_try, dtype=np.int64)) % q
            diff_try = [int(bc[i]) - int(b_np[i]) for i in range(m)]
            diff_try = [d if d <= q // 2 else d - q for d in diff_try]
            s_try, errs_try = refine(s_try, diff_try, max_iter=500)
            
            if errs_try == 0:
                s = s_try
                found = True
                print(f"[✓] 扰动#{trial} 成功!")
                break
        
        if not found:
            raise RuntimeError(f"恢复失败, errs={errs_try}")

print("[Final] AES-CBC 解密...")
s_bytes = bytes(s)
key = sha256(s_bytes).digest()
cipher = AES.new(key, AES.MODE_CBC, iv)
plain = cipher.decrypt(ct)
flag = unpad(plain, AES.block_size)
print("FLAG:", flag.decode())
