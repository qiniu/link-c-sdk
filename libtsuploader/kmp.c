#include <stdio.h>
#include <string.h>
#include "kmp.h"

int LinkInitKmp(LinkKMP *pKmp, const unsigned char *pattern, int patternSize) {
	memset(pKmp, 0, sizeof(LinkKMP));
	pKmp->pattern = pattern;
	pKmp->patternSize = patternSize;

	int len_p = patternSize;
	if (len_p < 2) {
		fprintf(stderr, "at least two byte\n");
		return -1;
	}
	if (len_p > 32) {
		fprintf(stderr, "exceed 32 byte\n");
		return -2;
	}

	int *t =(int*) &pKmp->prefix;
	t[0] = -1;
	t[1] =  0;

	int pos = 2, count = 0;
	for (;pos < len_p;) {

		if (pattern[pos-1] == pattern[count]) {
			count++;
			t[pos] = count;
			pos++;
		} else {
			if (count > 0) {
				count = t[count];
			} else {
				t[pos] = 0;
				pos++;
			}
		}
	}
	return 0;
}

int LinkFindPatternIndex(LinkKMP *pKmp, const unsigned char*s, int sLen) {
	if (sLen < pKmp->patternSize) {
		return -1;
	}
	int m = 0, i = 0;
	for (;m+i < sLen;) {
		if (pKmp->pattern[i] == s[m+i]) {
			if (i == pKmp->patternSize-1) {
				return m;
			}
			i++;
		} else {
			m = m + i - pKmp->prefix[i];
			if (pKmp->prefix[i] > -1) {
				i = pKmp->prefix[i];
			} else {
				i = 0;
			}
		}
	}
	return -2;
}
