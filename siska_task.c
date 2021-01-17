
#include"siska_task.h"
#include"siska_mm.h"
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

	asm volatile(
		"movl %0, %%cr3\n\t"
		"jmp 1f\n\t"
		"1:\n\t"
		:
		:"r"(0)
		:
	);

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

static void _set_mm_readonly(siska_task_t* task, unsigned long esp3)
{
	unsigned long* dir  = (unsigned long*) task->cr3;

	unsigned long i;
	unsigned long j;

	for (i = 0; i < 1024; i++) {
		if (!(dir[i] & PG_FLAG_PRESENT))
			continue;

		// kernel space < 1M
		if (i > (1u << 20) >> 22 && i < esp3 >> 22) {
			dir[i] &= ~PG_FLAG_WRITE;
			continue;
		}

		unsigned long* table = (unsigned long*)(dir[i] & ~(PG_SIZE - 1));

		for (j = 0; j < 1024; j++) {
			if (!(table[j] & PG_FLAG_PRESENT))
				continue;

			unsigned long vaddr = (i << 22) | (j << 12);

			if (esp3 & ~(PG_SIZE - 1) <= vaddr && vaddr < task->esp3)
				continue;

			if (vaddr >= 1u << 20) {
				table[i] &= ~PG_FLAG_WRITE;
				continue;
			}
#if 0
			if (esp3 & ~(PG_SIZE - 1) <= vaddr && vaddr < task->esp3)
				table[i] &= ~PG_FLAG_WRITE;
#endif
		}
	}
}

int _copy_memory(siska_task_t* child, siska_task_t* parent, unsigned long esp3)
{
	unsigned long* child_dir  = (unsigned long*)child->cr3;
	unsigned long* parent_dir = (unsigned long*)parent->cr3;

	siska_memcpy(child_dir, parent_dir, PG_SIZE);
#if 1
	unsigned long vaddr;

	for (vaddr = esp3 & ~(PG_SIZE - 1); vaddr < parent->esp3; vaddr += PG_SIZE) {

		unsigned long i =  vaddr >> 22;
		unsigned long j = (vaddr >> 12) & 0x3ff;

		if (!(parent_dir[i] & PG_FLAG_PRESENT))
			continue;

		unsigned long* child_table  = (unsigned long*) ( child_dir[i] & ~(PG_SIZE - 1));
		unsigned long* parent_table = (unsigned long*) (parent_dir[i] & ~(PG_SIZE - 1));

		if (child_table == parent_table) {
			siska_page_t* pg  = NULL;

			if (siska_get_free_pages(&pg, 1) < 0)
				return -1;

			child_table  = (unsigned long*)siska_page_addr(pg);
			child_dir[i] = (unsigned long )child_table | 0x7;

			siska_memcpy(child_table, parent_table, PG_SIZE);
		}

		if (!(parent_table[j] & PG_FLAG_PRESENT))
			continue;

		siska_page_t* pg2 = NULL;
		if (siska_get_free_pages(&pg2, 1) < 0)
			return -1;

		unsigned long child_paddr  = siska_page_addr(pg2);
		unsigned long parent_paddr = parent_table[j] & ~(PG_SIZE - 1);

		child_table[j] = child_paddr | 0x7;

		siska_memcpy((void*)child_paddr, (void*)parent_paddr, PG_SIZE);
	}
#endif
//	_set_mm_readonly(parent, esp3);
//	_set_mm_readonly(child,  esp3);
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
	if (siska_get_free_pages(&pg, 2) < 0)
		return -1;

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
		goto error;
	}

	siska_memcpy(child, current, PG_SIZE);

	child->cr3  = siska_page_addr(pg + 1);

	_copy_memory(child, current, regs->esp);

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

	siska_schedule();
error:
	return cpid;
}

int siska_kill(siska_regs_t* regs)
{
	int pid    = (int)regs->ebx;
	int signal = (int)regs->ecx;

	if (pid < 0 || pid >= SISKA_NB_TASKS)
		return -1;

	if (signal < 0 || signal >= SISKA_NB_SIGNALS)
		return -1;

	if (0 == signal) {
		if (siska_tasks[pid])
			return 0;
		return -1;
	}

	if (siska_tasks[pid]) {
		siska_tasks[pid]->signal_flags |= 1u << signal;
		return 0;
	}

	return -1;
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
//	siska_api_syscall(SISKA_SYSCALL_EXIT, 0, 0, 0);
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
	return 0;
}

int siska_do_signal(siska_regs_t* regs)
{
	int i;
	int count = 0;

	for (i = 1; i < SISKA_NB_SIGNALS; i++) {

		if (!(current->signal_flags & (1u << i)))
			continue;

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

int siska_exit(siska_regs_t* regs)
{
	return 0;
}

int siska_wait(siska_regs_t* regs)
{
	__siska_wait();
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

