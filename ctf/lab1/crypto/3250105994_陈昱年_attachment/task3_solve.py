"""
Task 3: Common Modulus Attack (共模攻击)
===========================================

Challenge:
    m = int.from_bytes(flag)
    n = sympy.nextprime(m >> 150)
    c_i = pow(m, e_i, n)   for each e_i in "AAA😍".encode()

Given ciphertexts:
    [62672938275009705596581242847130514775891540954531116,  # e=65 ('A')
     62672938275009705596581242847130514775891540954531116,  # e=65 ('A')
     62672938275009705596581242847130514775891540954531116,  # e=65 ('A')
     28065112754348826692839224620054962177237340625508142,  # e=240 (😍[0])
     82805428369361023445247061843917815511685788893461488,  # e=159 (😍[1])
     64529102835595887450014112173830620877278761152089439,  # e=152 (😍[2])
     61692909713449333722171394128578104844776919632656810]  # e=141 (😍[3])

Flag: AAA{C0mM0n_m0dUlUs_4tt4ck_1s_1nt3r3st1ng}
"""

from math import gcd


# ============ Given Data ============

ciphertexts = [
    62672938275009705596581242847130514775891540954531116,
    62672938275009705596581242847130514775891540954531116,
    62672938275009705596581242847130514775891540954531116,
    28065112754348826692839224620054962177237340625508142,
    82805428369361023445247061843917815511685788893461488,
    64529102835595887450014112173830620877278761152089439,
    61692909713449333722171394128578104844776919632656810,
]

exponents = list("AAA😍".encode())  # [65, 65, 65, 240, 159, 152, 141]

c65  = ciphertexts[0]
c240 = ciphertexts[3]
c159 = ciphertexts[4]
c152 = ciphertexts[5]
c141 = ciphertexts[6]


# ============ Step 1: Recover n ============
# m^(e1*e2) ≡ c_e1^e2 ≡ c_e2^e1 (mod n)
# => n divides |c_e1^e2 - c_e2^e1|
# Take gcd of multiple such differences to recover n

pairs = [
    (c65, 65, c240, 240),
    (c65, 65, c159, 159),
    (c65, 65, c152, 152),
    (c65, 65, c141, 141),
    (c240, 240, c159, 159),
    (c240, 240, c152, 152),
]

n = 0
for ca, ea, cb, eb in pairs:
    diff = pow(ca, eb) - pow(cb, ea)
    n = gcd(n, diff) if n else diff

print(f"[Step 1] Recovered n = {n}")
print(f"          n bit length = {n.bit_length()}")


# ============ Step 2: Common Modulus Attack ============
# Choose e1=65 and e2=152 with gcd(65, 152)=1
# Find x, y s.t. 65*x + 152*y = 1
# Then m = c65^x * c152^y (mod n)

def egcd(a, b):
    if b == 0:
        return a, 1, 0
    g, x, y = egcd(b, a % b)
    return g, y, x - (a // b) * y


def modinv(a, m):
    g, x, _ = egcd(a, m)
    return x % m


e1, e2 = 65, 152
g, x, y = egcd(e1, e2)
assert g == 1

# Handle negative exponents with modular inverse
if x < 0:
    term1 = pow(modinv(c65, n), -x, n)
else:
    term1 = pow(c65, x, n)

if y < 0:
    term2 = pow(modinv(c152, n), -y, n)
else:
    term2 = pow(c152, y, n)

m_mod_n = (term1 * term2) % n
assert pow(m_mod_n, 65, n) == c65
print(f"[Step 2] m mod n = {m_mod_n}")
print(f"          Verified: pow(m_mod_n, 65, n) == c65")


# ============ Step 3: Recover full m ============
# We know:
#   n = nextprime(t) where t = m >> 150
#   m = t * 2^150 + r, 0 <= r < 2^150
#
#   m mod n = (t * 2^150 + r) mod n = v
#
# With t = n - d:
#   r = (v + d * 2^150) mod n
#
# The wrap-around point (where v + d*2^150 >= n):
#   d = ceil((n - v) / 2^150)

v = m_mod_n
two150 = 1 << 150
d_wrap = (n - v + two150 - 1) // two150

print(f"[Step 3] d_wrap = {d_wrap}")

for delta in range(-1000, 1000):
    d = d_wrap + delta
    t = n - d
    if t <= 0:
        continue

    r = (v - t * two150) % n
    if r >= two150:
        continue

    m = t * two150 + r
    if pow(m, 65, n) != c65:
        continue

    try:
        flag = m.to_bytes((m.bit_length() + 7) // 8, 'big').decode('ascii')
        print(f"\n{'='*50}")
        print(f"  Flag: {flag}")
        print(f"{'='*50}")
        break
    except UnicodeDecodeError:
        continue
