import base64

standard_alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'
alphabet_str = "bctf{DEPCmQqklUgj5yNBA93IHMYaVFiXedxroKsh4GuSvJW72OzwLR6Z8p01nT}"
fixed_point = "NslSBwm6YNHHNreCNsmojw8zY9nGVzep9NoJ5LHpH3b8NKnQlB2Ca{XzIxeUyR85Y{COjRD09P4mFEAAFACZlAo0jwnGBrj7UAbwYBHDjBDEjBlMY{DWkE46YrVtaKh6ABDdVLoty{5Gjr5DMrmSNAo05DVu5NjR93m9VEDqMfHcjr5rAr2WVPo0lBVGaxA{N3mvBw8cYzbL5DHLlB8GBxrzYNo95B52N9HCIR4Ol3mcaxm3AocWkE2tArVeF{D0NxlvVrm6UElHFKmklB5CNE2tU{VtFPmp9B8WUNAtlNmUUBefyACwNR7zY9leFEwzj9nxARvDks5caoHo5sv{k{8ZYPH9aKwRABASy{HqY9vSjxAfNrBOVDA0As5EBEL7UBCZALAUj35SNKLAABmoHsocj6VUBxVp9NoSHBHDV64GU98ZjAmR5Evzl9leYweWl3lRFRv0UD4HB{X8IBv6BrDtjAl8Fwe29NoRBR4Zj9HUarmgAr5ck{DzlBAt5B76ABmf9rDkjAAGkzANNrvkjBDtY9nCaRnxMxH9ABmZAsb8NKnNy{mC5DIRl{VAUNeMMrDvBxADVs4SU92o93VoAR7zYDmEBsI7ABjwy{m09femjsIRjsm{VE4cYfeEVRBOUB8WBrmylD4GUBeAI3mvHwDOl9luFw5pY{mcVfAtlzHcjKLKU3m{MR4cjBVuFrDUNsmRMR4t5P5HU98fIAmJF{LZyfocjKLMj3ldU{8qYr88FRLflB5mBK20ArDANKAlN3m{VrmZjB59az5NANrOYzrzAKncBxVp9B5v5BmzyNVcjK46F3vvkfoKH9LGawoyMslvFLHDlElHHKADAomtNLAtUDVtaKL8Y{jw5LAtHBDmkRLRyPV{NR78YPAEVzA793vvILHKj35SFrexMrvkyLBzY3vlBx5xMrm9VBHO5DAUaRLyNACwVfAjlBeAUNeKyBDvyrH0ND48HKnxMrvkN9hzYE5AFze293CZU{Hy"

# 验证字母表
print("=== 字母表验证 ===")
print(f"长度: {len(alphabet_str)} (应为64)")
print(f"唯一字符数: {len(set(alphabet_str))} (应为64)")

# 检查是否所有标准 base64 字符都被映射
for c in standard_alphabet:
    if c not in set(alphabet_str):
        print(f"标准字符 '{c}' 不在字母表中?")
    # 注意：字母表是目标，标准是源。检查所有标准字符在字母表中的位置

# 构建翻译表
translation_table = str.maketrans(standard_alphabet, alphabet_str)

# 验证固定点属性：encode(fp) 的前 1000 字符应等于 fp[:1000]
def encode(s):
    return base64.b64encode(s.encode()).decode().replace('=', '').translate(translation_table)

encoded_fp = encode(fixed_point)
match = sum(1 for i in range(1000) if encoded_fp[i] == fixed_point[i])
print(f"\n不动点验证: {match}/1000")

# 完整字母表
print(f"\n完整字母表: {alphabet_str}")

# 提取 flag 内容
flag_start = alphabet_str.find('{')
flag_end = alphabet_str.find('}')
if flag_start != -1 and flag_end != -1:
    flag = alphabet_str[flag_start:flag_end+1]
    print(f"\nFLAG: {flag}")

# 同时验证：用字母表本身作为起始字符串会发生什么？
print("\n=== 用字母表作为起始字符串 ===")
current = alphabet_str
for i in range(10):
    old = current
    current = encode(current)
    check_len = min(200, len(current), len(old))
    if current[:check_len] == old[:check_len]:
        print(f"第 {i+1} 次迭代收敛！")
        match = sum(1 for j in range(min(200, len(current), len(fixed_point))) 
                   if current[j] == fixed_point[j])
        print(f"与目标不动点前200匹配: {match}/200")
        break
else:
    print("10次后未收敛")
    match = sum(1 for j in range(min(200, len(current), len(fixed_point))) 
               if current[j] == fixed_point[j])
    print(f"与目标不动点前200匹配: {match}/200")

# 把字母表写成 flag 形式
print("\n" + "="*50)
print("FLAG = " + alphabet_str)
