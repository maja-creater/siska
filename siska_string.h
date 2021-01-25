#ifndef SISKA_STRING_H
#define SISKA_STRING_H

static inline int siska_strcmp(const unsigned char* s0, const unsigned char* s1)
{
	while (*s0 && *s1) {

		if (*s0 < *s1)
			return -1;
		else if (*s0 > *s1)
			return 1;

		s0++;
		s1++;
	}

	if (*s0 == *s1)
		return 0;

	return *s0 < *s1 ? -1 : 1;
}

static inline int siska_strncmp(const unsigned char* s0, const unsigned char* s1, unsigned int n)
{
	unsigned int len = 0;

	while (*s0 && *s1 && len < n) {

		if (*s0 < *s1)
			return -1;
		else if (*s0 > *s1)
			return 1;

		s0++;
		s1++;
		len++;
	}

	if (len == n)
		return 0;

	return *s0 < *s1 ? -1 : 1;
}

static inline unsigned int siska_strlen(const unsigned char* s)
{
	const unsigned char* p = s;
	while (*p)
		p++;
	return (unsigned int)(p - s);
}

static inline unsigned char* siska_strncpy(unsigned char* dst, const unsigned char* src, unsigned int n)
{
	unsigned int len = 0;

	while (*src && len < n) {
		*dst++ = *src++;
		len++;
	}

	if (len < n)
		*dst = '\0';
	return dst;
}

#endif

