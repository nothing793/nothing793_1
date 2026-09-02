text_list = ' !"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\t\n'
char_to_idx = {c: i for i, c in enumerate(text_list)}


def extended_gcd(a, b):
    if a == 0:
        return b, 0, 1
    g, x, y = extended_gcd(b % a, a)
    return g, y - (b // a) * x, x


def modinv(a, m):
    g, x, _ = extended_gcd(a % m, m)
    if g != 1:
        return None
    return x % m


with open('cipher.txt', 'r') as f:
    cipher = f.read()

KEY_LEN = 29


def show_position(pos, k_filter=None):
    """显示某个位置在所有 k 值下的解密结果"""
    chars = [cipher[i] for i in range(len(cipher)) if i % KEY_LEN == pos]
    print(f"\n位置 {pos}（共 {len(chars)} 个字符）")
    print(f"{'k':>4}  解密结果")
    print("-" * 60)

    if k_filter is not None:
        ks = [k_filter]
    else:
        ks = range(1, 97)

    for k in ks:
        inv_k = modinv(k, 97)
        if inv_k is None:
            continue
        decrypted = []
        for c in chars:
            ci = char_to_idx[c]
            pi = (ci * inv_k) % 97
            decrypted.append(text_list[pi])
        result = ''.join(decrypted)
        disp = result[:50].replace('\n', '\\n').replace('\t', '\\t')
        print(f"{k:>4}  {disp}")
    print()


def decrypt_full(key):
    """用完整密钥解密全文"""
    plain = []
    for i, c in enumerate(cipher):
        ci = char_to_idx[c]
        ki = key[i % len(key)]
        inv_k = modinv(ki, 97)
        if inv_k is None:
            plain.append('?')
            continue
        pi = (ci * inv_k) % 97
        plain.append(text_list[pi])
    return ''.join(plain)


def main():
    print("=" * 60)
    print("Vigenere 乘法密码 — 手动解密工具")
    print(f"密文长度: {len(cipher)} 字符")
    print(f"密钥长度: {KEY_LEN}")
    print("=" * 60)
    print("命令：")
    print("  pos <0-28>          查看某位置所有 96 种解密")
    print("  pos <0-28> k <1-96> 只看某位置某 k 值")
    print("  key 51,14,22,...    用完整密钥解密全文")
    print("  quit                退出")
    print()

    while True:
        try:
            cmd = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            break

        if not cmd:
            continue

        parts = cmd.split()

        if parts[0] == 'quit':
            break

        elif parts[0] == 'pos':
            if len(parts) < 2:
                print("用法: pos <0-28> [k <1-96>]")
                continue
            pos = int(parts[1])
            if pos < 0 or pos >= KEY_LEN:
                print(f"位置必须在 0~{KEY_LEN-1}")
                continue
            if len(parts) >= 4 and parts[2] == 'k':
                k = int(parts[3])
                show_position(pos, k)
            else:
                show_position(pos)

        elif parts[0] == 'key':
            try:
                key = [int(x) for x in parts[1].split(',')]
            except:
                print("用法: key 51,14,22,90,...")
                continue
            result = decrypt_full(key)
            print(f"\n密钥: {key}")
            print(f"长度: {len(key)}")
            print("-" * 60)
            print(result)
            print("-" * 60)
            print()

        else:
            print("未知命令，输入 quit 退出")


if __name__ == '__main__':
    main()
