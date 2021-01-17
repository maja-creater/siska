#ifndef SISKA_TASK_H
#define SISKA_TASK_H

#include"siska_core.h"

typedef struct {
	unsigned long eax;
	unsigned long ebx;
	unsigned long ecx;
	unsigned long edx;

	unsigned long gs;
	unsigned long fs;
	unsigned long es;
	unsigned long ds;

	unsigned long eip;
	unsigned long cs;
	unsigned long eflags;
	unsigned long esp;
	unsigned long ss;
} siska_regs_t;

struct siska_tss_s
{
	unsigned long tss_prev;

	unsigned long esp0;
	unsigned long ss0;

	unsigned long esp1;
	unsigned long ss1;

	unsigned long esp2;
	unsigned long ss2;

	unsigned long cr3;
	unsigned long eip;
	unsigned long eflags;

	unsigned long eax;
	unsigned long ecx;
	unsigned long edx;
	unsigned long ebx;

	unsigned long esp;
	unsigned long ebp;

	unsigned long esi;
	unsigned long edi;

	unsigned long es;
	unsigned long cs;
	unsigned long ss;
	unsigned long ds;
	unsigned long fs;
	unsigned long gs;

	unsigned long ldt;

	unsigned long iomap;

	unsigned long ssp;
};

struct siska_task_s
{
	siska_list_t list;

	int pid;
	int ppid;

#define SISKA_TASK_RUNNING 1
#define SISKA_TASK_SLEEP   2
#define SISKA_TASK_ZOMBIE  3
	int status;

	unsigned long   eip;

	unsigned long   esp0;
	unsigned long   ss0;

	unsigned long   esp3;

	unsigned long   cr3;

	siska_mm_t*     mm;

	unsigned long   signal_flags;
	void          (*signal_handlers[SISKA_NB_SIGNALS])(int signal);
};

void siska_task_init();

void siska_schedule();
int  siska_fork(siska_regs_t* regs);

int  siska_exit(siska_regs_t* regs);
int  siska_wait(siska_regs_t* regs);

int  siska_kill(siska_regs_t* regs);
int  siska_signal(siska_regs_t* regs);

int  siska_getpid();
int  siska_getppid();

#endif

