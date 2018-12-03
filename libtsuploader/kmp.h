#ifndef __TTOOL_LinkKmp_H__
#define __TTOOL_LinkKmp_H__

typedef struct {
	const unsigned char * pattern;
	int patternSize;
	int prefix[32];
	int prefixLen;
}LinkKMP;

int LinkInitKmp(LinkKMP *pKmp, const unsigned char *pattern, int patternSize);
int LinkFindPatternIndex(LinkKMP *pKmp, const unsigned char*s, int sLen);

#endif
