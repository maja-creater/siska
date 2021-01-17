#include"siska_def.h"

int siska_ulong2a(char* buf, int* pn, int size, unsigned long num)
{
	int n = *pn;
	int i = n;

	while (n < size - 1) {

		buf[n++] = num % 10 + '0';

		num /= 10;
		if (0 == num)
			break;
	}

	*pn = n--;

	while (i < n) {
		char c   = buf[i];
		buf[i++] = buf[n];
		buf[n--] = c;
	}
	return *pn;
}

int siska_long2a(char* buf, int* pn, int size, long num)
{
	int n = *pn;

	if (n < size - 1 && num < 0) {
		buf[n++] = '-';
		num = -num;
	}

	*pn = n;
	return siska_ulong2a(buf, pn, size, num);
}

int siska_double2a(char* buf, int* pn, int size, double num)
{
	if (*pn < size - 1 && num < 0.0) {
		buf[(*pn)++] = '-';
		num = -num;
	}

	long l    = (long)num;
	long diff = (long)((num - l) * 1e6);

	siska_ulong2a(buf, pn, size, l);

	if (*pn < size - 1)
		buf[(*pn)++] = '.';

	return siska_ulong2a(buf, pn, size, diff);
}

int siska_hex2a(char* buf, int* pn, int size, unsigned long num)
{
	int n = *pn;
	int i = n;

	while (n < size - 1) {

		unsigned char x = num % 16;

		buf[n++] = x > 9 ? x - 10 + 'a' : x + '0';

		num /= 16;
		if (0 == num)
			break;
	}

	*pn = n--;

	while (i < n) {
		char c   = buf[i];
		buf[i++] = buf[n];
		buf[n--] = c;
	}
	return *pn;
}

int siska_hex2a_prefix(char* buf, int* pn, int size, unsigned long num)
{
	int n = *pn;

	if (n < size - 1 - 2) {
		buf[n++] = '0';
		buf[n++] = 'x';
	}

	*pn = n;
	return siska_hex2a(buf, pn, size, num);
}

int siska_p2a(char* buf, int* pn, int size, unsigned long num)
{
	if (0 == num) {
		int   n = *pn;
		char* p = "null";

		while (n < size - 1 && *p)
			buf[n++] = *p++;

		*pn = n;
		return n;
	}

	return siska_hex2a_prefix(buf, pn, size, num);
}

int siska_str2a(char* buf, int* pn, int size, const char* str)
{
	int n = *pn;

	while (n < size - 1 && *str)
		buf[n++] = *str++;

	*pn = n;
	return n;
}

int siska_vsnprintf(char* buf, int size, const char* fmt, siska_va_list ap)
{
	int n = 0;

	while (n < size - 1 && *fmt) {

		if ('%' != *fmt) {
			buf[n++] = *fmt++;
			continue;
		}

		fmt++;
		if ('%' == *fmt) {
			buf[n++] = *fmt++;
			continue;
		}

		int prefix = 0;
		if ('#' == *fmt) {
			prefix++;
			fmt++;
		}

		int l = 0;
		if ('l' == *fmt) {
			l++;
			fmt++;
		}

		switch (*fmt) {
			case 'c':
				buf[n++] = siska_va_arg(ap, int);
				break;
			case 'd':
				if (l)
					siska_long2a(buf, &n, size, siska_va_arg(ap, long));
				else
					siska_long2a(buf, &n, size, siska_va_arg(ap, int));
				break;
			case 'u':
				if (l)
					siska_ulong2a(buf, &n, size, siska_va_arg(ap, unsigned long));
				else
					siska_ulong2a(buf, &n, size, siska_va_arg(ap, unsigned int));
				break;
			case 'x':
				if (prefix) {
					if (l)
						siska_hex2a_prefix(buf, &n, size, siska_va_arg(ap, unsigned long));
					else
						siska_hex2a_prefix(buf, &n, size, siska_va_arg(ap, unsigned int));
				} else {
					if (l)
						siska_hex2a(buf, &n, size, siska_va_arg(ap, unsigned long));
					else
						siska_hex2a(buf, &n, size, siska_va_arg(ap, unsigned int));
				}
				break;
			case 'p':
				siska_p2a(buf, &n, size, siska_va_arg(ap, unsigned long));
				break;
			case 'f':
			case 'g':
				siska_double2a(buf, &n, size, siska_va_arg(ap, double));
				break;
			case 's':
				siska_str2a(buf, &n, size, siska_va_arg(ap, char*));
				break;
			default:
				return -1;
				break;
		}

		fmt++;
	}

	buf[n] = '\0';
	return n;
}

int siska_snprintf(char* buf, int size, const char* fmt, ...)
{
	siska_va_list ap;

	siska_va_start(ap, fmt);
	int n = siska_vsnprintf(buf, size, fmt, ap);
	siska_va_end(ap);

	return n;
}
#if 0
int main()
{
	char buf[1024];
	siska_snprintf(buf, 1023, "i: %d, ld: %ld,  x: %x, x: %#lx, p: %p, s: %s, f: %f, d: %lg\n",
			100, -255ll, 254, 253, buf, "hello", 2.71828, -3.1415926);

	printf("%s\n", buf);
	printf("%p\n", buf);
	return 0;
}
#endif
