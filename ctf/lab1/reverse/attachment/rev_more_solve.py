
import struct
import ctypes

ROUNDS = 111
DELTA = 0x4A531AA9  # 1246986025


def u32(x):
    return ctypes.c_uint32(x).value


def add(a, b):
    return u32(a + b)


def sub(a, b):
    return u32(a - b)


def enc(v, key):
    """加密一个8字节块"""
    v0, v1 = v
    _sum = 0

    for _ in range(ROUNDS):
        # v15 += (cap_high + ((16*cap_high) ^ (cap_high>>5))) ^ (v18 + key[v18 & 3])
        x = u32(v1 << 4) ^ u32(v1 >> 5)
        x = add(x, v1)
        y = add(_sum, key[_sum & 3])
        v0 = add(v0, x ^ y)

        _sum = add(_sum, DELTA)

        # cap_high += (v15 + ((16*v15) ^ (v15>>5)))
        #        ^ (v18 + key[((v18+DELTA)>>11) & 3] + DELTA)
        x = u32(v0 << 4) ^ u32(v0 >> 5)
        x = add(x, v0)
        k_idx = u32((_sum + DELTA) >> 11) & 3
        y = add(_sum, key[k_idx])
        y = add(y, DELTA)
        v1 = add(v1, x ^ y)

    return v0, v1


def dec(v, key):
    """解密一个8字节块"""
    v0, v1 = v
    _sum = u32(DELTA * ROUNDS)

    for _ in range(ROUNDS):
        # 逆第二轮
        x = u32(v0 << 4) ^ u32(v0 >> 5)
        x = add(x, v0)
        k_idx = u32((_sum + DELTA) >> 11) & 3
        y = add(_sum, key[k_idx])
        y = add(y, DELTA)
        v1 = sub(v1, x ^ y)

        _sum = sub(_sum, DELTA)

        # 逆第一轮
        x = u32(v1 << 4) ^ u32(v1 >> 5)
        x = add(x, v1)
        y = add(_sum, key[_sum & 3])
        v0 = sub(v0, x ^ y)

    return v0, v1


def process(data_hex, key, func):
    """处理整个hex字符串"""
    data = bytes.fromhex(data_hex)
    result = b''
    for i in range(0, len(data), 8):
        block = data[i:i+8]
        v0, v1 = struct.unpack('>2I', block)
        r0, r1 = func((v0, v1), key)
        result += struct.pack('>2I', r0, r1)
    return result.hex()


if __name__ == '__main__':
    key_hex = "16aad828f71522a7e9e59051ef3d093f"
    cipher_hex = "8134fe511f2673f1"

    print("=" * 60)
    print("rev_more XTEA 解密工具 v4")
    print("=" * 60)

    key = struct.unpack('>4I', bytes.fromhex(key_hex))
    print(f"\n[密钥] {key_hex}")
    for i, k in enumerate(key):
        print(f"  k{i}={k:08X}")

    # 验证加密
    plain_hex = b'AAAAAAAA'.hex()
    enc_hex = process(plain_hex, key, enc)
    ok = "✅ 匹配!" if enc_hex == cipher_hex else "❌ 不匹配"
    print(f"\n[验证] 加密 'AAAAAAAA'")
    print(f"  结果: {enc_hex}")
    print(f"  期望: {cipher_hex}")
    print(f"  {ok}")

    # 解密
    dec_hex = process(cipher_hex, key, dec)
    dec_bytes = bytes.fromhex(dec_hex)
    print(f"\n[解密] {cipher_hex}")
    print(f"  hex:   {dec_hex}")
    print(f"  ascii: {dec_bytes}")

    # 同时重新加密验证
    rt_hex = process(dec_hex, key, enc)
    print(f"\n[验证] 解密结果重新加密")
    print(f"  {rt_hex}")
    print(f"  {'✅ 一致!' if rt_hex == cipher_hex else '❌ 不一致'}")
