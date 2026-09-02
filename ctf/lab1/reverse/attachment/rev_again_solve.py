#!/usr/bin/env python3
"""
rev_again.exe 解密脚本
算法：魔改 RC4（cipher_crypt 中 j 的计算多了 +1）
密钥：kEy3rd
密文：从 IDA 中提取的 44 字节
"""

def rc4_mod_crypt(key: bytes, data: bytes) -> bytes:
    """
    魔改 RC4 加解密
    与标准 RC4 的区别：PRGA 中 j 的计算多了 +1
    反编译对应逻辑：
        v8 = (v8 + 1) & 0xFF        # i
        v7 = (S[v8] + v7 + 1) & 0xFF  # j (多了+1!)
        swap(S[v8], S[v7])
        output = S[(S[v8] + S[v7]) & 0xFF] ^ data[i]
    """
    # KSA - Key Scheduling Algorithm（标准 RC4 KSA）
    S = list(range(256))
    j = 0
    for i in range(256):
        j = (j + S[i] + key[i % len(key)]) & 0xFF
        S[i], S[j] = S[j], S[i]

    # PRGA - Pseudo-Random Generation Algorithm（魔改版）
    result = bytearray()
    v8 = 0  # i
    v7 = 0  # j
    for byte in data:
        v8 = (v8 + 1) & 0xFF                    # i = i + 1
        v7 = (S[v8] + v7 + 1) & 0xFF            # j = S[i] + j + 1  ← 魔改！
        S[v8], S[v7] = S[v7], S[v8]             # swap
        k = S[(S[v8] + S[v7]) & 0xFF]
        result.append(byte ^ k)

    return bytes(result)


if __name__ == '__main__':
    # 从 IDA 中提取的密文（44 字节）
    cipher_hex = '50f78cf98059a87785cacb896f561a6f2f24fd84678e8f36d4377f36dbd0ecc9ecd904039ced5390b5fde512'
    cipher = bytes.fromhex(cipher_hex)

    key = b'kEy3rd'

    print('=' * 60)
    print('rev_again.exe - 魔改 RC4 解密')
    print('=' * 60)
    print(f'密钥: {key.decode()}')
    print(f'密文长度: {len(cipher)} 字节')

    plaintext = rc4_mod_crypt(key, cipher)
    print(f'\n标准 44 字节解密: {plaintext!r}')

    # 去掉尾部零
    stripped = plaintext.rstrip(b'\x00')
    print(f'去零后解密: {stripped!r}')

    try:
        flag = stripped.decode('utf-8')
        print(f'\n✅✅✅ Flag: {flag}')
    except UnicodeDecodeError:
        # 尝试不同长度
        for cut in range(0, 6):
            pt = rc4_mod_crypt(key, cipher[:len(cipher)-cut])
            s = pt.rstrip(b'\x00')
            try:
                flag = s.decode('utf-8')
                print(f'\n✅ Flag (密文前{len(cipher)-cut}字节): {flag}')
                break
            except:
                pass
        else:
            print(f'\n所有尝试均失败，拉丁解码:')
            for cut in range(0, 6):
                pt = rc4_mod_crypt(key, cipher[:len(cipher)-cut])
                s = pt.rstrip(b'\x00')
                print(f'  尾部减{cut}: {s.decode("latin-1")} [hex: {s.hex()}]')
