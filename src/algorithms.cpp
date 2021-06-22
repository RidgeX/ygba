#include <assert.h>
#include <stdlib.h>

#include "algorithms.h"

bool is_power_of_2(uint32_t x) {
    return (x & (x - 1)) == 0;
}

uint32_t next_power_of_2(uint32_t x) {
    return (x == 1 ? 1 : 1 << (32 - bits_clz(x - 1)));
}

uint32_t bits_clz(uint32_t x) {
#ifdef __GNUC__
    return __builtin_clz(x);
#else
    uint32_t t = ((x & 0xffff0000) == 0) << 4;
    x >>= 16 - t;
    uint32_t r = t;
    t = ((x & 0xff00) == 0) << 3;
    x >>= 8 - t;
    r += t;
    t = ((x & 0xf0) == 0) << 2;
    x >>= 4 - t;
    r += t;
    t = ((x & 0xc) == 0) << 1;
    x >>= 2 - t;
    r += t;
    return r + ((2 - x) & -((x & 2) == 0));
#endif
}

uint32_t bits_ctz(uint32_t x) {
#ifdef __GNUC__
    return __builtin_ctz(x);
#else
    uint32_t t = ((x & 0x0000ffff) == 0) << 4;
    x >>= t;
    uint32_t r = t;
    t = ((x & 0x00ff) == 0) << 3;
    x >>= t;
    r += t;
    t = ((x & 0x0f) == 0) << 2;
    x >>= t;
    r += t;
    t = ((x & 0x3) == 0) << 1;
    x >>= t;
    x &= 3;
    r += t;
    return r + ((2 - (x >> 1)) & -((x & 1) == 0));
#endif
}

uint32_t bits_popcount(uint32_t x) {
#ifdef __GNUC__
    return __builtin_popcount(x);
#else
    x = x - ((x >> 1) & 0x55555555);
    x = ((x >> 2) & 0x33333333) + (x & 0x33333333);
    x = (x + (x >> 4)) & 0x0f0f0f0f;
    x = (x + (x >> 16));
    return (x + (x >> 8)) & 0x0000003f;
#endif
}

static int max(int a, int b) {
    return (a < b ? b : a);
}

static unsigned char *reverse(unsigned char *pattern, int m) {
    unsigned char *rev_pattern = (unsigned char *) calloc(m, sizeof(unsigned char));
    assert(rev_pattern != NULL);

    for (int i = 0; i < m; i++) {
        rev_pattern[m - 1 - i] = pattern[i];
    }

    return rev_pattern;
}

static int *compute_prefix(unsigned char *pattern, int m) {
    int *prefix = (int *) calloc(m, sizeof(int));
    assert(prefix != NULL);

    prefix[0] = 0;
    int pos = 0;
    for (int i = 1; i < m; i++) {
        while (pos > 0 && pattern[pos] != pattern[i]) {
            pos = prefix[pos - 1];
        }
        if (pattern[pos] == pattern[i]) {
            pos++;
        }
        prefix[i] = pos;
    }

    return prefix;
}

unsigned char *knuth_morris_pratt_matcher(unsigned char *text, int n, unsigned char *pattern, int m) {
    int *prefix = compute_prefix(pattern, m);
    unsigned char *match = NULL;

    int pos = 0;
    for (int i = 0; i < n; i++) {
        while (pos > 0 && pattern[pos] != text[i]) {
            pos = prefix[pos - 1];
        }
        if (pattern[pos] == text[i]) {
            pos++;
        }
        if (pos == m) {
            match = &text[i - m + 1];
            break;
            //pos = prefix[pos - 1];
        }
    }

    free(prefix);
    return match;
}

static int *compute_last_occurrence(unsigned char *pattern, int m) {
    int *last_occurrence = (int *) calloc(256, sizeof(int));
    assert(last_occurrence != NULL);

    for (int i = 0; i < 256; i++) {
        last_occurrence[i] = 0;
    }
    for (int i = 0; i < m; i++) {
        last_occurrence[pattern[i]] = i + 1;
    }

    return last_occurrence;
}

static int *compute_good_suffix(unsigned char *pattern, int m) {
    int *prefix = compute_prefix(pattern, m);
    unsigned char *rev_pattern = reverse(pattern, m);
    int *rev_prefix = compute_prefix(rev_pattern, m);
    int *good_suffix = (int *) calloc(m + 1, sizeof(int));
    assert(good_suffix != NULL);

    for (int i = 0; i <= m; i++) {
        good_suffix[i] = m - prefix[m - 1];
    }
    for (int i = 1; i <= m; i++) {
        int j = m - rev_prefix[i - 1];
        if (good_suffix[j] > i - rev_prefix[i - 1]) {
            good_suffix[j] = i - rev_prefix[i - 1];
        }
    }

    free(prefix);
    free(rev_pattern);
    free(rev_prefix);
    return good_suffix;
}

unsigned char *boyer_moore_matcher(unsigned char *text, int n, unsigned char *pattern, int m) {
    int *last_occurrence = compute_last_occurrence(pattern, m);
    int *good_suffix = compute_good_suffix(pattern, m);
    unsigned char *match = NULL;

    int shift = 0;
    while (shift <= n - m) {
        int pos = m;
        while (pos > 0 && pattern[pos - 1] == text[shift + pos - 1]) {
            pos--;
        }
        if (pos == 0) {
            match = &text[shift];
            break;
            //shift += good_suffix[pos];
        } else {
            shift += max(good_suffix[pos], pos - last_occurrence[text[shift + pos - 1]]);
        }
    }

    free(last_occurrence);
    free(good_suffix);
    return match;
}
