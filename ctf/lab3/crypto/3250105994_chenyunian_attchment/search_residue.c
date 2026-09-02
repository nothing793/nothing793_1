
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static unsigned char is_w[256];

static void init_w(void) {
    int i;
    for (i = 0; i < 256; i++) is_w[i] = 0;
    for (i = 48; i <= 57; i++) is_w[i] = 1;
    for (i = 65; i <= 90; i++) is_w[i] = 1;
    for (i = 97; i <= 122; i++) is_w[i] = 1;
    is_w[95] = 1;
}

static const unsigned char chars_63[63] = {
    '0','1','2','3','4','5','6','7','8','9',
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    '_',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z'
};

/* d = 13^37 (137 bits = 18 bytes)
   middle_int is 23 bytes (184 bits)
   target is the desired middle_int mod d
   we search: middle = target - k*d where k <= 0
   or equivalently: k_pos = -k, middle = target + k_pos*d */

static uint64_t D[3]; /* d in 184-bit LE */
static uint64_t T[3]; /* target in 184-bit LE */

/* d mod 2^40 */
static uint64_t d_mod_40;
static uint64_t target_mod_40;
static uint64_t inv_d_mod_40;

/* Check middle = target + kp*d for all \w bytes */
/* kp = -k, k is the original k (negative) */
/* kp in [0, MAX_KP] */
static int check_middle(uint64_t kp, unsigned char *out_bytes) {
    /* kp*d (in 184-bit) */
    unsigned __int128 p0 = (unsigned __int128)kp * D[0];
    uint64_t kd0 = (uint64_t)p0;
    uint64_t carry0 = (uint64_t)(p0 >> 64);
    
    unsigned __int128 p1 = (unsigned __int128)kp * D[1] + carry0;
    uint64_t kd1 = (uint64_t)p1;
    uint64_t carry1 = (uint64_t)(p1 >> 64);
    
    unsigned __int128 p2 = (unsigned __int128)kp * D[2] + carry1;
    uint64_t kd2 = (uint64_t)p2 & 0x00FFFFFFFFFFFFFFULL;
    
    /* middle = T + kd */
    unsigned __int128 s0 = (unsigned __int128)T[0] + kd0;
    uint64_t r0 = (uint64_t)s0;
    uint64_t c0 = (uint64_t)(s0 >> 64);
    
    unsigned __int128 s1 = (unsigned __int128)T[1] + kd1 + c0;
    uint64_t r1 = (uint64_t)s1;
    uint64_t c1 = (uint64_t)(s1 >> 64);
    
    unsigned __int128 s2 = (unsigned __int128)T[2] + kd2 + c1;
    uint64_t r2 = (uint64_t)(s2 & 0x00FFFFFFFFFFFFFFULL);
    
    /* Check bytes. Big-endian order:
       r2 bits 48-55 = byte 0 (MSB of middle)
       r2 bits 40-47 = byte 1
       r2 bits 32-39 = byte 2
       r2 bits 24-31 = byte 3
       r2 bits 16-23 = byte 4
       r2 bits 8-15  = byte 5
       r2 bits 0-7   = byte 6
       r1 bits 56-63 = byte 7
       r1 bits 48-55 = byte 8
       ...
       r1 bits 0-7   = byte 14
       r0 bits 56-63 = byte 15
       ...
       r0 bits 0-7   = byte 22 (LSB) */
    
    uint64_t t;
    t = r2;
    if (!is_w[(t >> 48) & 0xFF]) return 0; /* byte 0 */
    if (!is_w[(t >> 40) & 0xFF]) return 0;
    if (!is_w[(t >> 32) & 0xFF]) return 0;
    if (!is_w[(t >> 24) & 0xFF]) return 0;
    if (!is_w[(t >> 16) & 0xFF]) return 0;
    if (!is_w[(t >> 8) & 0xFF])  return 0;
    if (!is_w[(t >> 0) & 0xFF])  return 0; /* byte 6 */
    
    t = r1;
    if (!is_w[(t >> 56) & 0xFF]) return 0; /* byte 7 */
    if (!is_w[(t >> 48) & 0xFF]) return 0;
    if (!is_w[(t >> 40) & 0xFF]) return 0;
    if (!is_w[(t >> 32) & 0xFF]) return 0;
    if (!is_w[(t >> 24) & 0xFF]) return 0;
    if (!is_w[(t >> 16) & 0xFF]) return 0;
    if (!is_w[(t >> 8) & 0xFF])  return 0;
    if (!is_w[(t >> 0) & 0xFF])  return 0; /* byte 14 */
    
    t = r0;
    if (!is_w[(t >> 56) & 0xFF]) return 0; /* byte 15 */
    if (!is_w[(t >> 48) & 0xFF]) return 0;
    if (!is_w[(t >> 40) & 0xFF]) return 0;
    if (!is_w[(t >> 32) & 0xFF]) return 0;
    if (!is_w[(t >> 24) & 0xFF]) return 0;
    if (!is_w[(t >> 16) & 0xFF]) return 0;
    if (!is_w[(t >> 8) & 0xFF])  return 0;
    if (!is_w[(t >> 0) & 0xFF])  return 0; /* byte 22 */
    
    /* Extract bytes for output */
    out_bytes[0]  = (r2 >> 48) & 0xFF;
    out_bytes[1]  = (r2 >> 40) & 0xFF;
    out_bytes[2]  = (r2 >> 32) & 0xFF;
    out_bytes[3]  = (r2 >> 24) & 0xFF;
    out_bytes[4]  = (r2 >> 16) & 0xFF;
    out_bytes[5]  = (r2 >> 8) & 0xFF;
    out_bytes[6]  = r2 & 0xFF;
    out_bytes[7]  = (r1 >> 56) & 0xFF;
    out_bytes[8]  = (r1 >> 48) & 0xFF;
    out_bytes[9]  = (r1 >> 40) & 0xFF;
    out_bytes[10] = (r1 >> 32) & 0xFF;
    out_bytes[11] = (r1 >> 24) & 0xFF;
    out_bytes[12] = (r1 >> 16) & 0xFF;
    out_bytes[13] = (r1 >> 8) & 0xFF;
    out_bytes[14] = r1 & 0xFF;
    out_bytes[15] = (r0 >> 56) & 0xFF;
    out_bytes[16] = (r0 >> 48) & 0xFF;
    out_bytes[17] = (r0 >> 40) & 0xFF;
    out_bytes[18] = (r0 >> 32) & 0xFF;
    out_bytes[19] = (r0 >> 24) & 0xFF;
    out_bytes[20] = (r0 >> 16) & 0xFF;
    out_bytes[21] = (r0 >> 8) & 0xFF;
    out_bytes[22] = r0 & 0xFF;
    
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 7) {
        fprintf(stderr, 
            "Usage: %s <t0_hex> <t1_hex> <t2_hex> "
            "<d0_hex> <d1_hex> <d2_hex> "
            "[d_mod40_hex] [target_mod40_hex] [inv_d_mod40_hex] "
            "[k_min] [k_max]\n"
            "Default k: [%lld, 0]\n", argv[0],
            -149147221370893LL);
        return 1;
    }
    
    init_w();
    
    T[0] = strtoull(argv[1], NULL, 16);
    T[1] = strtoull(argv[2], NULL, 16);
    T[2] = strtoull(argv[3], NULL, 16);
    D[0] = strtoull(argv[4], NULL, 16);
    D[1] = strtoull(argv[5], NULL, 16);
    D[2] = strtoull(argv[6], NULL, 16);
    
    if (argc >= 10) {
        d_mod_40 = strtoull(argv[7], NULL, 16);
        target_mod_40 = strtoull(argv[8], NULL, 16);
        inv_d_mod_40 = strtoull(argv[9], NULL, 16);
    } else {
        /* Compute from d and target */
        d_mod_40 = D[0] & 0xFFFFFFFFFFULL;
        target_mod_40 = T[0] & 0xFFFFFFFFFFULL;
        /* Compute inv of d_mod_40 mod 2^40 using extended Euclid */
        uint64_t a = d_mod_40, b = 1ULL << 40, t;
        uint64_t x0 = 1, x1 = 0;
        while (a > 1) {
            uint64_t q = a / (b % a);
            /* ... too complex, just compute in Python */
        }
        inv_d_mod_40 = 0; /* will be set from Python */
    }
    
    long long k_range_start, k_range_end;
    if (argc >= 12) {
        k_range_start = atoll(argv[10]);
        k_range_end = atoll(argv[11]);
    } else {
        k_range_start = 0;
        k_range_end = 0; /* all */
    }
    
    /* Actually let's take a different approach:
       Enumerate all 63^5 valid low-5-byte patterns,
       for each find the k residue mod 2^40,
       then iterate the ~135 k values */
    
    printf("Optimized search by residue enumeration\n");
    printf("T = [%016llx %016llx %016llx]\n", T[2], T[1], T[0]);
    printf("D = [%016llx %016llx %016llx]\n", D[2], D[1], D[0]);
    fflush(stdout);
    
    long long MAX_KP = 149147221370893LL;
    long long STEP = 1LL << 40;  /* 2^40 */
    
    long long total_checked = 0;
    int found = 0;
    long long found_kp = 0;
    unsigned char found_bytes[23];
    
    /* Precompute w_low for computing low_int from 5 bytes */
    uint64_t w_low[5] = {256ULL*256*256*256, 256ULL*256*256, 256ULL*256, 256, 1};
    
    for (int i0 = 0; i0 < 63 && !found; i0++) {
        unsigned char b0 = chars_63[i0];
        for (int i1 = 0; i1 < 63 && !found; i1++) {
            unsigned char b1 = chars_63[i1];
            for (int i2 = 0; i2 < 63 && !found; i2++) {
                unsigned char b2 = chars_63[i2];
                for (int i3 = 0; i3 < 63 && !found; i3++) {
                    unsigned char b3 = chars_63[i3];
                    for (int i4 = 0; i4 < 63 && !found; i4++) {
                        unsigned char b4 = chars_63[i4];
                        
                        /* low_int = b0*256^4 + ... + b4 */
                        uint64_t low_int = (uint64_t)b0 * w_low[0] + 
                                           (uint64_t)b1 * w_low[1] + 
                                           (uint64_t)b2 * w_low[2] + 
                                           (uint64_t)b3 * w_low[3] + 
                                           (uint64_t)b4;
                        
                        /* k_mod = (target_mod - low_int) * inv_d_mod  (mod 2^40) */
                        uint64_t k_mod = ((target_mod_40 - low_int) & 0xFFFFFFFFFFULL) * inv_d_mod_40;
                        k_mod &= 0xFFFFFFFFFFULL;
                        
                        /* kp values in [0, MAX_KP] with kp ≡ k_mod (mod 2^40) */
                        /* Note: kp = -k, and k ≡ k_mod (mod 2^40) meaning 
                           k = k_mod + m * 2^40 for some m
                           Since k ∈ [-MAX_KP, 0], kp = -k = -k_mod - m*2^40
                           So kp = (-k_mod mod 2^40) + m * 2^40 */
                        uint64_t kp_mod = ((-(int64_t)k_mod) & 0xFFFFFFFFFFULL);
                        
                        /* First kp = kp_mod, then kp = kp_mod + 2^40, 2*2^40, ... */
                        uint64_t kp = kp_mod;
                        while (kp <= (uint64_t)MAX_KP) {
                            total_checked++;
                            if (check_middle(kp, found_bytes)) {
                                found = 1;
                                found_kp = (long long)kp;
                                break;
                            }
                            kp += STEP;
                        }
                        
                        if (total_checked % 500000000 == 0) {
                            printf("  checked %lld, i0=%d\n", total_checked, i0);
                            fflush(stdout);
                        }
                    }
                }
            }
        }
    }
    
    printf("Total checked: %lld\n", total_checked);
    if (found) {
        printf("*** FOUND at kp=%lld ***\n", found_kp);
        printf("Flag: SEE{");
        for (int i = 0; i < 23; i++) putchar(found_bytes[i]);
        printf("}\n");
        fflush(stdout);
    }
    
    return found ? 0 : 1;
}
