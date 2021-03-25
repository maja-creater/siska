#ifndef SISKA_API_H
#define SISKA_API_H

#include"siska_def.h"
#include"siska_list.h"

static inline int siska_api_syscall(unsigned long num, unsigned long arg0, unsigned long arg1, unsigned long arg2)
{
	int ret;

	asm volatile(
		"int $0x80\n\t"
		:"=a"(ret)
		:"a"(num), "b"(arg0), "c"(arg1), "d"(arg2)
		:
	);
	return ret;
}

int siska_api_printf(const char* fmt, ...);
int siska_api_execve(const char* filename, char* const argv, char* const envp);

static inline int siska_api_getpid()
{
	return siska_api_syscall(SISKA_SYSCALL_GETPID, 0, 0, 0);
}

static inline int siska_api_getppid()
{
	return siska_api_syscall(SISKA_SYSCALL_GETPPID, 0, 0, 0);
}


#endif

