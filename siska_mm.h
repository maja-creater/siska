#ifndef SISKA_MM_H
#define SISKA_MM_H

#include"siska_core.h"

struct siska_mm_s
{
};

typedef struct {
	siska_spinlock_t lock;

	int refs;
} siska_page_t;

typedef struct {
	union {
		siska_list_t list;
		long size_used;
	};
	long size_free;
} siska_block_t;

typedef struct {
	siska_list_t     head;
	siska_spinlock_t lock;
} siska_block_head_t;

#define PG_FLAG_PRESENT 0x1
#define PG_FLAG_WRITE   0x2

extern siska_page_t* siska_pages;

void siska_mm_init();

int  siska_get_free_pages(siska_page_t** pages, int nb_pages);
int  siska_free_pages    (siska_page_t*  pages, int nb_pages);

static inline void siska_ref_page(siska_page_t* pg)
{
	if (pg) {
		unsigned long flags;
		siska_spin_lock_irqsave(&pg->lock, flags);
		pg->refs++;
		siska_spin_unlock_irqrestore(&pg->lock, flags);
	}
}
static inline void siska_unref_page(siska_page_t* pg)
{
	if (pg) {
		unsigned long flags;
		siska_spin_lock_irqsave(&pg->lock, flags);
		pg->refs--;
		siska_spin_unlock_irqrestore(&pg->lock, flags);
	}
}

static inline unsigned long siska_page_addr(siska_page_t* page)
{
	unsigned long index = (unsigned long)(page - siska_pages);
	unsigned long addr  = index << PG_SHIFT;
	return addr;
}

static inline siska_page_t* siska_addr_page(unsigned long addr)
{
	return siska_pages + (addr >> PG_SHIFT);
}
#if 0
void* siska_kmalloc(int size);
void* siska_krealloc(void* p, int size);
void  siska_kfree(void* p);

void  siska_memcpy(void* dst, const    void* src, unsigned long size);
void  siska_memset(void* dst, unsigned long data, unsigned long size);
#else
#include<stdlib.h>
#include<string.h>
#define siska_kmalloc  malloc
#define siska_krealloc realloc
#define siska_kfree    free 
#define siska_memcpy   memcpy 
#define siska_memset   memset
#endif

int   siska_copy_memory(siska_task_t* child, siska_task_t* parent, unsigned long esp3);
void  siska_free_memory(siska_task_t* task);

#endif

