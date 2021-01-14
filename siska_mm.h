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

void siska_mm_init();

int  siska_get_free_pages(siska_page_t** pages, int nb_pages);
int  siska_free_pages    (siska_page_t*  pages, int nb_pages);

unsigned long siska_page_addr(siska_page_t* page);

void* siska_kmalloc(int size);
void* siska_krealloc(void* p, int size);
void  siska_kfree(void* p);

void  siska_memcpy(void* dst, void* src, unsigned long size);
void  siska_memset(void* dst, unsigned long data, unsigned long size);

#endif

