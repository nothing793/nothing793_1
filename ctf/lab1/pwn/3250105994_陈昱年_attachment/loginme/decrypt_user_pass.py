import struct

VERIFY = bytes([
    0x27, 0x85, 0x56, 0x4a, 0xb2, 0x29, 0xe7, 0xf1,
    0xa6, 0xc0, 0xab, 0xd7, 0xd6, 0x82, 0xb8, 0x1b,
    0x4c, 0x43, 0xb0, 0x33, 0x0d, 0xb2, 0xbe, 0xb8,
    0x10, 0x7a, 0x73, 0x30, 0x0a, 0xf3, 0xff, 0x59
])

KEY = b'\xaa' * 16

def tea_decrypt_block(v0, v1, k):
    delta = 0x9e3779b9
    s = 0xC6EF3720  # delta * 32
    k0, k1, k2, k3 = struct.unpack('<4I', k)
    
    for _ in range(32):
        v1 = (v1 - (((v0 << 4) + k2) ^ (v0 + s) ^ ((v0 >> 5) + k3))) & 0xFFFFFFFF
        v0 = (v0 - (((v1 << 4) + k0) ^ (v1 + s) ^ ((v1 >> 5) + k1))) & 0xFFFFFFFF
        s = (s - delta) & 0xFFFFFFFF
    
    return v0, v1

def tea_decrypt(data, key):
    result = b''
    for i in range(0, len(data), 8):
        block = data[i:i+8]
        v0, v1 = struct.unpack('<2I', block)
        v0, v1 = tea_decrypt_block(v0, v1, key)
        result += struct.pack('<2I', v0, v1)
    return result

plaintext = tea_decrypt(VERIFY, KEY)
print(f"Decrypted password (hex): {plaintext.hex()}")
print(f"Decrypted password (raw): {plaintext}")
# Strip null bytes and print as string
print(f"Decrypted password (printable): {plaintext.rstrip(b'\x00').decode('latin-1')}")
