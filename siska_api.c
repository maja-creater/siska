
#include"siska_api.h"

int siska_api_printf(const char* fmt, ...)
{
	char buf[128];

	siska_va_list ap;

	siska_va_start(ap, fmt);
	int n = siska_vsnprintf(buf, sizeof(buf), fmt, ap);
	siska_va_end(ap);

	if (n > 0)
		return siska_api_syscall(SISKA_SYSCALL_WRITE, (unsigned long)buf, n, 0);
	return n;
}

