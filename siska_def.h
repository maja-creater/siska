#ifndef SISKA_DEF_H
#define SISKA_DEF_H

#define ON_BOCHS
#ifdef ON_BOCHS
#define NULL ((void*)0)

typedef char* siska_va_list;
#define INTSIZEOF(n)         ((sizeof(n) + sizeof(int) - 1) & ~(sizeof(int) - 1))
#define siska_va_start(ap,v) (ap = (siska_va_list)&v + INTSIZEOF(v))
#define siska_va_arg(ap,t)   (*(t*)((ap += INTSIZEOF(t)) - INTSIZEOF(t)))
#define siska_va_end(ap)     (ap = (siska_va_list)0)

#else
#include<stdio.h>
#include<stdarg.h>
#define siska_va_list  va_list
#define siska_va_start va_start
#define siska_va_arg   va_arg
#define siska_va_end   va_end
#endif

#include"siska_atomic.h"
#include"siska_string.h"

#define SISKA_SYSCALL_FORK    1
#define SISKA_SYSCALL_SCHED   2
#define SISKA_SYSCALL_WRITE   3
#define SISKA_SYSCALL_KILL    4
#define SISKA_SYSCALL_SIGNAL  5
#define SISKA_SYSCALL_EXIT    6
#define SISKA_SYSCALL_WAIT    7
#define SISKA_SYSCALL_GETPID  8
#define SISKA_SYSCALL_GETPPID 9
#define SISKA_SYSCALL_EXECVE  10

#define SISKA_SIGINT          2
#define SISKA_SIGKILL         9
#define SISKA_SIGSEGV         11
#define SISKA_SIGCHLD         17
#define SISKA_NB_SIGNALS      (sizeof(unsigned long) * 8)
#define SISKA_SIG_DFL         NULL
#define SISKA_SIG_IGN         ((void*)0x1)

int siska_vsnprintf(char* buf, int size, const char* fmt, siska_va_list ap);
int siska_snprintf (char* buf, int size, const char* fmt, ...);

#endif

