
#include"siska_task.h"
#include"siska_mm.h"
#include"siska_vfs.h"
#include"siska_api.h"

#define SISKA_NB_TASKS  16

static siska_task_t*    siska_tasks[SISKA_NB_TASKS];

static siska_spinlock_t siska_task_lock;

static siska_list_t     siska_task_head;



void siska_task_init()
{
	siska_memset(siska_tasks, 0, sizeof(siska_task_t*) * SISKA_NB_TASKS);

	siska_spinlock_init(&siska_task_lock);

	siska_list_init(&siska_task_head);

	siska_tasks[0] = get_asm_addr(_task0);

	siska_list_add_tail(&siska_task_head, &(siska_tasks[0]->list));
}

void siska_switch_to(siska_task_t* prev, siska_task_t* next)
{
	siska_tss_t* tss = get_asm_addr(_tss0);

	switch_to_pgdir(PGDIR_KERNEL);

	asm volatile(
		"pushl %%ebp\n\t"
		"pushfl\n\t"
		"movl %%esp, %0\n\t"
		"movl $1f, %1\n\t"
		"movl %2, %%esp\n\t"
		"addl $0x1000, %5\n\t"
		"movl %5, 4(%6)\n\t"
		"movl %4, %%cr3\n\t"
		"jmp *%3\n\t"
		"1:\n\t"
		"popfl\n\t"
		"popl %%ebp\n\t"
		:"=m"(prev->esp0), "=m"(prev->eip)
		:"r"(next->esp0), "r"(next->eip), "r"(next->cr3), "r"(next), "r"(tss)
		:"esp", "ebp", "flags"
	);
}

void siska_schedule()
{
	siska_list_t* l;
	siska_task_t* next;
	siska_task_t* parent;

	unsigned long flags;
	siska_spin_lock_irqsave(&siska_task_lock, flags);
#if 1
	for (l = siska_list_head(&siska_task_head);
			l != siska_list_sentinel(&siska_task_head);
			l = siska_list_next(l)) {

		next = siska_list_data(l, siska_task_t, list);

//		if (SISKA_TASK_SLEEP == next->status)
//			continue;

		if (next != current)
			break;
	}

	if (l == siska_list_sentinel(&siska_task_head))
		next = siska_tasks[0];
#else
	siska_list_del(&current->list);
	siska_list_add_tail(&siska_task_head, &current->list);

	l    = siska_list_head(&siska_task_head);
	next = siska_list_data(l, siska_task_t, list);
#endif
#if 0
	if (next->ppid >= 0) {
		parent = siska_tasks[next->ppid];
		if (SISKA_TASK_SLEEP == parent->status)
			parent->status = SISKA_TASK_RUNNING;
	}
#endif
	siska_spin_unlock_irqrestore(&siska_task_lock, flags);

	if (next == current)
		return;

	siska_switch_to(current, next);
}

int siska_fork(siska_regs_t* regs)
{
	siska_task_t* child;
	siska_page_t* pg;

	unsigned long  esp = (unsigned long)regs;
	unsigned long* dir;
	int cpid = -1;
	int i;

	pg = NULL;
	if (siska_get_free_pages(&pg, 2) < 0) {
		siska_printk("fork error0!\n");
		return -1;
	}

	child = (siska_task_t*) siska_page_addr(pg);

	current->status = SISKA_TASK_SLEEP;

	unsigned long flags;
	siska_spin_lock_irqsave(&siska_task_lock, flags);
	for (i = 1; i < SISKA_NB_TASKS; i++) {
		if (!siska_tasks[i]) {
			cpid = i;
			siska_tasks[i] = child;
			break;
		}
	}
	siska_spin_unlock_irqrestore(&siska_task_lock, flags);

	if (-1 == cpid) {
		siska_free_pages(pg, 2);
		current->status = SISKA_TASK_RUNNING;
		siska_printk("fork error1!\n");
		goto error;
	}

	siska_memcpy(child, current, PG_SIZE);

	child->cr3  = siska_page_addr(pg + 1);

	if (siska_copy_memory(child, current, regs->esp) < 0) {
		siska_free_pages(pg, 2);
		current->status = SISKA_TASK_RUNNING;
		siska_printk("fork error2!\n");
		goto error;
	}

	child->pid  = cpid;
	child->ppid = current->pid;
	child->eip  = (unsigned long)get_asm_addr(_ret_from_fork);
	child->esp0 = esp;
	child->status = SISKA_TASK_RUNNING;

	child->signal_flags = 0;
	siska_memset(child->signal_handlers, (unsigned long)SISKA_SIG_DFL, SISKA_NB_SIGNALS * sizeof(void*));

	siska_spin_lock_irqsave(&siska_task_lock, flags);
	siska_list_add_front(&siska_task_head, &child->list);
	siska_spin_unlock_irqrestore(&siska_task_lock, flags);

	siska_printk("fork ok!\n");

	siska_schedule();
error:
	return cpid;
}

int __siska_kill(int pid, int signal)
{
//	siska_printk("kill, pid: %d, signal: %d\n", pid, signal);
	if (pid < 0 || pid >= SISKA_NB_TASKS)
		return -1;

	if (signal < 0 || signal >= SISKA_NB_SIGNALS)
		return -1;

	if (0 == signal) {
		if (siska_tasks[pid])
			return 0;
		return -1;
	}

	unsigned long flags;
	siska_spin_lock_irqsave(&siska_task_lock, flags);
	if (siska_tasks[pid]) {

		switch_to_pgdir(PGDIR_KERNEL);
		siska_tasks[pid]->signal_flags |= 1u << signal;
		switch_to_pgdir(current->cr3);

		if (SISKA_SIGCHLD == signal)
			siska_printk("kill, pid: %d, siska_tasks[%d]: %p, signal: %d\n", pid, pid, siska_tasks[pid], signal);
		siska_spin_unlock_irqrestore(&siska_task_lock, flags);
		return 0;
	}

	siska_spin_unlock_irqrestore(&siska_task_lock, flags);
	return -1;
}

int siska_kill(siska_regs_t* regs)
{
	int pid    = (int)regs->ebx;
	int signal = (int)regs->ecx;

	return __siska_kill(pid, signal);
}

int siska_signal(siska_regs_t* regs)
{
	int   signal  =   (int)regs->ebx;
	void* handler = (void*)regs->ecx;

	if (signal < 0 || signal >= SISKA_NB_SIGNALS)
		return -1;

	current->signal_handlers[signal] = handler;
	return 0;
}

void _signal_handler_default(int sig)
{
	int pid = siska_api_syscall(SISKA_SYSCALL_GETPID, 0, 0, 0);
	if (0 != pid)
		siska_api_printf("pid %d, received signal %d\n", pid, sig);
	siska_api_syscall(SISKA_SYSCALL_EXIT, 0, 0, 0);
}

void _signal_handler_kill(int sig)
{
	siska_api_syscall(SISKA_SYSCALL_EXIT, 0, 0, 0);
}

void __siska_do_signal(siska_regs_t* regs, int sig, void (*handler)(int sig), int count)
{
	void* entry = get_asm_addr(_signal_handler_entry);

	if (0 == count) {
		regs->esp -= sizeof(long);

		*(unsigned long*)regs->esp = regs->eip;
	}

	*(long* )(regs->esp - 1 * sizeof(long)) = sig;
	*(void**)(regs->esp - 2 * sizeof(long)) = handler;
	*(void**)(regs->esp - 3 * sizeof(long)) = entry;

	regs->esp -= 3 * sizeof(long);
}

int __siska_wait()
{
	int i;
	for (i = 1; i < SISKA_NB_TASKS; i++) {

		siska_task_t* task;
		siska_page_t* pg;
		unsigned long flags;

		siska_spin_lock_irqsave(&siska_task_lock, flags);
		task = siska_tasks[i];
		if (!task || SISKA_TASK_ZOMBIE != task->status) {
			siska_spin_unlock_irqrestore(&siska_task_lock, flags);
			continue;
		}

		siska_tasks[i] = NULL;
		siska_spin_unlock_irqrestore(&siska_task_lock, flags);

		siska_printk("wait, parent: %d, child: %d, exit_code: %d\n", current->pid, task->pid, task->exit_code);

		pg = siska_addr_page((unsigned long)task);
		siska_printk("pg, refs: %d\n", pg->refs);
		siska_printk("pg + 1, refs: %d\n", (pg + 1)->refs);
		siska_free_pages(pg, 2);

		siska_printk("pg, refs: %d\n", pg->refs);
		siska_printk("pg + 1, refs: %d\n", (pg + 1)->refs);
		pg = NULL;
	}

	return 0;
}

int siska_do_signal(siska_regs_t* regs)
{
	int i;
	int count = 0;

	for (i = 1; i < SISKA_NB_SIGNALS; i++) {

		if (!(current->signal_flags & (1u << i)))
			continue;

		if (SISKA_SIGCHLD == i)
			siska_printk("do signal, pid: %d, signal: %d\n", current->pid, i);

		current->signal_flags &= ~(1u << i);

		void* handler = current->signal_handlers[i];

		if (SISKA_SIGCHLD == i) {
			__siska_wait();
			continue;
		}

		if (SISKA_SIGKILL == i) {
			__siska_do_signal(regs, SISKA_SIGKILL, _signal_handler_kill, count);
			count++;
			break;
		}

		if (SISKA_SIG_IGN == handler)
			continue;

		if (SISKA_SIG_DFL == handler)
			__siska_do_signal(regs, i, _signal_handler_default, count);
		else
			__siska_do_signal(regs, i, handler, count);
		count++;
	}

	if (count > 0) {
		regs->esp += sizeof(long);
		regs->eip  = (unsigned long)get_asm_addr(_signal_handler_entry);
	}

	return 0;
}

void siska_exit(siska_regs_t* regs)
{
	if (0 == current->pid)
		return;

	current->exit_code = (int)regs->ebx;
	current->status    = SISKA_TASK_ZOMBIE;

	siska_printk("exit, pid: %d, ppid: %d, code: %d\n", current->pid, current->ppid, current->exit_code);
#if 1
	unsigned long flags;
	siska_spin_lock_irqsave(&siska_task_lock, flags);
	siska_list_del(&current->list);
	__siska_kill(current->ppid, SISKA_SIGCHLD);
	siska_spin_unlock_irqrestore(&siska_task_lock, flags);

	siska_free_memory(current);
	siska_schedule();
#endif
}

int siska_wait(siska_regs_t* regs)
{
	__siska_wait();
	return 0;
}

int siska_execve(siska_regs_t* regs)
{
	if (0 == current->pid)
		return -1;

	char* filename = (char*)regs->ebx;

	siska_printk("execve, filename: %s\n", filename);

	siska_file_t* file = NULL;

	int ret = siska_vfs_open(&file, filename,
			SISKA_FILE_FILE | SISKA_FILE_FLAG_R | SISKA_FILE_FLAG_W,
			0777);
	if (ret < 0) {
		siska_printk("open %s, ret: %d\n", filename, ret);
		return -1;
	}

	ret = siska_vfs_read(file, (char*)0x800000, 1024);
	if (ret < 0) {
		siska_printk("read %s error, ret: %d\n", filename, ret);
		return -1;
	}
	siska_printk("read %s, ret: %d\n", filename, ret);

	current->code3 = 0x800000;
	current->data3 = 0x800000 + ret;
	current->heap3 = (current->data3 + PG_SIZE - 1) >> PG_SHIFT << PG_SHIFT;
	current->brk3  = current->heap3;

	current->ebp3  = 0x900000;
	current->end3  = current->ebp3;

	regs->esp = 0x900000;
	regs->eip = 0x800000;
	return 0;
}

int siska_getpid()
{
	return current->pid;
}

int siska_getppid()
{
	return current->ppid;
}

