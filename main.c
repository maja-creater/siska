
#include"siska_task.h"
#include"siska_mm.h"
#include"siska_vfs.h"
#include"siska_api.h"

void siska_console_init();

int siska_write(siska_regs_t* regs)
{
	const char* fmt = (const char*)regs->ebx;

	return siska_console_write(fmt);
}

int _do_timer()
{
	siska_schedule();
	return 0;
}

int _do_syscall_default()
{
	siska_printk("syscall default!\n");
	return 0;
}

void _syscall_init()
{
	int i;
	for (i = 0; i < 256; i++)
		set_syscall_handler(i, _do_syscall_default);

	set_syscall_handler(SISKA_SYSCALL_FORK,    siska_fork);
	set_syscall_handler(SISKA_SYSCALL_SCHED,   siska_schedule);

	set_syscall_handler(SISKA_SYSCALL_WRITE,   siska_write);

	set_syscall_handler(SISKA_SYSCALL_KILL,    siska_kill);
	set_syscall_handler(SISKA_SYSCALL_SIGNAL,  siska_signal);

	set_syscall_handler(SISKA_SYSCALL_EXIT,    siska_exit);
	set_syscall_handler(SISKA_SYSCALL_WAIT,    siska_wait);

	set_syscall_handler(SISKA_SYSCALL_GETPID,  siska_getpid);
	set_syscall_handler(SISKA_SYSCALL_GETPPID, siska_getppid);

	set_syscall_handler(SISKA_SYSCALL_EXECVE,  siska_execve);
}

int _main()
{
	siska_tss_t*  tss0  = get_asm_addr(_tss0);
	siska_task_t* task0 = get_asm_addr(_task0);

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

	siska_console_init();

	_syscall_init();

	siska_task_init();
	siska_mm_init();
	siska_fs_init();

#if 0
	siska_file_t* file = NULL;

	int ret = siska_vfs_open(&file, "/home/execve",
			SISKA_FILE_FILE | SISKA_FILE_FLAG_R | SISKA_FILE_FLAG_W,
			0777);
	siska_printk("open /home/execve, ret: %d\n", ret);
	if (ret < 0) {
		return -1;
	}
#if 1
	char buf[64];
	ret = siska_vfs_read(file, buf, sizeof(buf) - 1);
	if (ret < 0) {
		siska_printk("read /home/execve error, ret: %d\n", ret);
		return -1;
	}
	siska_printk("/home/execve, ret: %d, buf: %s\n", ret, buf);
#endif
#endif

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

	task0->code3  = (unsigned long)task0 + sizeof(siska_task_t);
	task0->data3  = (unsigned long)task0 + sizeof(siska_task_t);
	task0->heap3  = (unsigned long)task0 + sizeof(siska_task_t);
	task0->brk3   = (unsigned long)task0 + sizeof(siska_task_t);
	task0->ebp3   = (unsigned long)task0 + PG_SIZE;
	task0->end3   = (unsigned long)task0 + PG_SIZE;

	task0->signal_flags = 0;
	siska_memset(task0->signal_handlers, (unsigned long)SISKA_SIG_DFL, SISKA_NB_SIGNALS * sizeof(void*));

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
	int cpid = siska_api_syscall(SISKA_SYSCALL_FORK, 0, 0, 0);
	if (-1 == cpid) {
		while (1) {
		}
	} else if (0 == cpid) {
		siska_api_execve(get_asm_addr(_execve_msg), 0, 0);
		while (1) {

//			siska_api_syscall(SISKA_SYSCALL_KILL, 0, SISKA_SIGINT, 0);
//			siska_api_printf(get_asm_addr(_fork1_msg));
//			volatile unsigned long* p = (volatile unsigned long*)(1u << 30);
//			*p = 0x1234;
//			siska_api_syscall(SISKA_SYSCALL_SCHED, 0, 0, 0);
		}
	} else {
		while (1) {
//			siska_api_printf(get_asm_addr(_fork0_msg));

//			siska_api_syscall(SISKA_SYSCALL_KILL, 1, SISKA_SIGINT, 0);
		}
	}
#endif
	return 0;
}

