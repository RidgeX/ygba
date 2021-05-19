#include <assert.h>
#include <stdlib.h>

static int max(int a, int b) {
    return (a < b ? b : a);
}

static unsigned char *reverse(unsigned char *pattern, int m) {
    unsigned char *rev_pattern = malloc(sizeof(unsigned char) * m);
    assert(rev_pattern != NULL);

    for (int i = 0; i < m; i++) {
        rev_pattern[m - 1 - i] = pattern[i];
    }

    return rev_pattern;
}

static int *compute_prefix(unsigned char *pattern, int m) {
    int *prefix = malloc(sizeof(int) * m);
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
    int *last_occurrence = malloc(sizeof(int) * 256);
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
    int *good_suffix = malloc(sizeof(int) * (m + 1));
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
