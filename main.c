
#include"siska_task.h"
#include"siska_mm.h"

volatile unsigned long _jiffies;

int _printk(const char* fmt)
{
	unsigned char* p = (unsigned char*)0xb8000;

	unsigned long flags;
	siska_irqsave(flags);
	while (*fmt) {
		*p++ = *fmt++;
		*p++ = 0x7f;
	}
	siska_irqrestore(flags);

	return 0;
}

int siska_write(unsigned long esp, const char* fmt)
{
	_printk(fmt);
	return 0;
}

int _do_timer()
{
	siska_schedule();
	return 0;
}

int _do_syscall_default()
{
	_printk("syscall default!\n");
	return 0;
}

void _syscall_init()
{
	int i;
	for (i = 0; i < 8; i++)
		set_syscall_handler(i, _do_syscall_default);

	set_syscall_handler(SISKA_SYSCALL_FORK,  siska_fork);
	set_syscall_handler(SISKA_SYSCALL_SCHED, siska_schedule);
	set_syscall_handler(SISKA_SYSCALL_WRITE, siska_write);
}

int _write(const char* fmt)
{
	int ret;
	asm volatile(
		"movl $3, %%eax\n\t"
		"movl %1, %%ebx\n\t"
		"int $0x80\n\t"
		"movl %%eax, %0\n\t"
		:"=r"(ret)
		:"r"(fmt)
		:"eax", "ebx"
	);
	return ret;
}

int _main()
{
	siska_tss_t*  tss0  = get_asm_addr(_tss0);
	siska_task_t* task0 = get_asm_addr(_task0);

//	set_trap_handler(SISKA_INTERRUPT_PAGE_FAULT, _page_fault_int_handler);
	set_intr_handler(SISKA_INTERRUPT_PAGE_FAULT, _page_fault_int_handler);

	_jiffies = 0;

#define HZ    100u
#define LATCH (1193180u / HZ)
#if 1
	outb_p(0x36, 0x43);
	outb_p(LATCH & 0xff, 0x40);
	outb(LATCH >> 8, 0x40);
	set_intr_handler(SISKA_INTERRUPT_TIMER, _timer_int_handler);
	outb(inb_p(0x21) & ~0x01, 0x21);
#endif
	set_trap_handler(SISKA_INTERRUPT_SYSCALL, _syscall_handler);

	_syscall_init();

	siska_task_init();
	siska_mm_init();

	tss0->esp0 = (unsigned long)task0 + PG_SIZE;
	tss0->ss0  = 0x10;
	tss0->cs   = 0xf;
	tss0->ds   = 0x17;
	tss0->es   = 0x17;
	tss0->fs   = 0x17;
	tss0->gs   = 0x17;
	tss0->ldt  = 0x28;
	tss0->iomap = 0x80000000;

	task0->pid  =  0;
	task0->ppid = -1;
	task0->cr3  =  0;
	task0->esp3 = (unsigned long)task0 + PG_SIZE;

	asm volatile(
		"pushfl\n\t"
		"andl $0xffffbfff, (%%esp)\n\t"
		"popfl\n\t"
		"movl $0x20, %%eax\n\t"
		"ltr  %%ax\n\t"
		"movl $0x28, %%eax\n\t"
		"lldt %%ax\n\t"
		"sti\n\t"
		:
		:
		:"eax"
	);

	asm volatile(
		"movl  %%esp, %%eax\n\t"
		"pushl $0x17\n\t"
		"pushl %%eax\n\t"
		"pushfl\n\t"
		"pushl $0xf\n\t"
		"pushl $1f\n\t"
		"iret\n\t"
		"1:\n\t"
		"movl $0x17, %%eax\n\t"
		"movl %%eax, %%ds\n\t"
		"movl %%eax, %%es\n\t"
		"movl %%eax, %%fs\n\t"
		"movl %%eax, %%gs\n\t"
		:
		:
		:"eax"
	);
#if 1
	int cpid = siska_syscall(SISKA_SYSCALL_FORK, 0, 0, 0);
	if (-1 == cpid) {
		while (1) {
		}
	} else if (0 == cpid) {
		while (1) {
			_write(get_asm_addr(_fork1_msg));

			volatile unsigned long* p = (volatile unsigned long*)(1u << 30);
			*p = 0x1234;
//			siska_syscall(SISKA_SYSCALL_SCHED, 0, 0, 0);
		}
	} else {
		while (1) {
			_write(get_asm_addr(_fork0_msg));
//			siska_syscall(SISKA_SYSCALL_SCHED, 0, 0, 0);
		}
	}
#endif
	return 0;
}

