#ifndef SISKA_CORE_H
#define SISKA_CORE_H

#include"siska_def.h"
#include"siska_list.h"

typedef struct siska_task_s siska_task_t;
typedef struct siska_tss_s  siska_tss_t;
typedef struct siska_mm_s   siska_mm_t;

#define PG_SHIFT 12
#define PG_SIZE  (1u << PG_SHIFT)

extern volatile unsigned long _jiffies;

#define SISKA_INTERRUPT_PAGE_FAULT 14
#define SISKA_INTERRUPT_TIMER      0x20
#define SISKA_INTERRUPT_SYSCALL    0x80

#define SISKA_SYSCALL_FORK  1
#define SISKA_SYSCALL_SCHED 2
#define SISKA_SYSCALL_WRITE 3

#define get_asm_addr(_id) \
	({ \
		void* addr; \
		asm volatile("lea "#_id", %0\n\t" :"=r"(addr)::); \
		addr; \
	})

static inline siska_task_t* get_current()
{
	siska_task_t* current;
	asm volatile(
		"andl %%esp, %0"
		:"=r"(current)
		:"0"(~(PG_SIZE - 1))
		:
	);
	return current;
}
#define current get_current()

#define set_gate_handler(num, type, handler) \
	do { \
		asm volatile( \
			"lea "#handler", %%edx\n\t" \
			"movl $0x00080000, %%eax\n\t" \
			"movw %%dx, %%ax\n\t" \
			"movw %1, %%dx\n\t" \
			"lea  _idt(, %0, 8), %0\n\t" \
			"movl %%eax, (%0)\n\t" \
			"movl %%edx, 4(%0)\n\t" \
			: \
			:"r"(num), "i"(type) \
			:"eax", "edx" \
		); \
	} while (0)
#define set_intr_handler(num, handler) set_gate_handler(num, 0x8e00, handler)
#define set_trap_handler(num, handler) set_gate_handler(num, 0xef00, handler)

#define set_syscall_handler(num, handler) \
	do { \
		asm volatile( \
			"lea "#handler", %%eax\n\t" \
			"movl %%eax, _syscall_table(, %0, 4)\n\t" \
			: \
			:"r"(num) \
			:"eax" \
		); \
	} while (0)

static inline void outb_p(unsigned char data, unsigned short port)
{
	asm volatile(
		"movb %0, %%al\n\t"
		"outb %%al, %1\n\t"
		"jmp 1f\n\t"
		"1: jmp 1f\n\t"
		"1:\n\t"
		:
		:"r"(data), "i"(port)
		:
	);
}
static inline void outb(unsigned char data, unsigned short port)
{
	asm volatile(
		"movb %0, %%al\n\t"
		"outb %%al, %1\n\t"
		:
		:"r"(data), "i"(port)
		:
	);
}
static inline unsigned char inb_p(unsigned short port)
{
	unsigned char data;
	asm volatile(
		"inb %1, %%al\n\t"
		"movb %%al, %0\n\t"
		"jmp 1f\n\t"
		"1: jmp 1f\n\t"
		"1:\n\t"
		:"=r"(data)
		:"i"(port)
		:
	);
	return data;
}

static inline int siska_syscall(unsigned long num, unsigned long arg0, unsigned long arg1, unsigned long arg2)
{
	int ret;

	asm volatile(
		"movl %1, %%eax\n\t"
		"movl %2, %%ebx\n\t"
		"movl %3, %%ecx\n\t"
		"movl %4, %%edx\n\t"
		"int $0x80\n\t"
		"movl %%eax, %0\n\t"
		:"=r"(ret)
		:"r"(num), "r"(arg0), "r"(arg1), "r"(arg2)
		:"eax", "ebx", "ecx", "edx"
	);
	return ret;
}

int _printk(const char* fmt);

#endif

