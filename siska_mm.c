#if 0
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#endif

#include"siska_mm.h"
#include"siska_task.h"

#define SISKA_KERNEL_SIZE (128 * 1024)

#define SISKA_MM_EXT_PTR  0x90002
#define SISKA_MM_EXT_KB   (*(volatile unsigned short*)SISKA_MM_EXT_PTR)
//#define SISKA_MM_EXT_KB   (4 * 4)
#define SISKA_MM_EXT_MAX_KB (15 * 1024)

siska_page_t* siska_pages        = (siska_page_t*)(1UL << 20);
unsigned long siska_total_pages  = 0;
unsigned long siska_used_pages   = 0;

#define BLOCK_SHIFT_MIN    5
static siska_block_head_t  siska_block_heads[PG_SHIFT];

static unsigned long base = 0;

static inline int siska_block_is_used(siska_block_t* b)
{
	return b->size_used & 0x1;
}

static inline void siska_block_set_used(siska_block_t* b)
{
	b->size_used |= 0x1;
}

static inline long siska_block_get_shift(long size)
{
	long n;
	asm volatile(
		"bsr %1, %0"
		:"=r"(n)
		:"r"(size)
		:
	);

	return n;
}

static inline siska_block_t* siska_block_head(void* p)
{
	return (siska_block_t*)((unsigned long)p - sizeof(long));
}

static inline void* siska_block_data(siska_block_t* b)
{
	return (void*)((unsigned long)b + sizeof(long));
}

static inline long siska_block_size(siska_block_t* b)
{
	long used = b->size_used & 0x1;
	return used ? b->size_used & ~0x1 : b->size_free;
}

static inline long siska_block_data_size(siska_block_t* b)
{
	long used = b->size_used & 0x1;
	return used ? (b->size_used & ~0x1) - sizeof(long) : 0;
}

static inline long siska_block_need_shift(long size)
{
	size += sizeof(long);
	long n;
	asm volatile(
		"bsr %1, %0"
		:"=r"(n)
		:"r"(size)
		:
	);

	n += !!(size & ~(1u << n));
	return n;
}

void siska_mm_init()
{
	siska_block_head_t* bh;

	unsigned long i;
	unsigned long mm_ext_KB = SISKA_MM_EXT_KB;

	if (mm_ext_KB > SISKA_MM_EXT_MAX_KB)
		mm_ext_KB = SISKA_MM_EXT_MAX_KB;

	siska_total_pages = (1024 + mm_ext_KB) >> (PG_SHIFT - 10);

	siska_used_pages  = (siska_total_pages * sizeof(siska_page_t) + PG_SIZE - 1) >> PG_SHIFT;
	siska_used_pages += 1u << 20 >> PG_SHIFT;

//	siska_printk("siska_pages: %p, total_pages: %d, used_pages: %d\n", siska_pages, siska_total_pages, siska_used_pages);

	for (i = 0; i < siska_total_pages; i++) {

		siska_spinlock_init(&(siska_pages[i].lock));

		if (i < siska_used_pages)
			siska_pages[i].refs = 1;
		else
			siska_pages[i].refs = 0;
	}

	for (i = 0; i < PG_SHIFT; i++) {
		bh = &siska_block_heads[i];

		siska_list_init(&bh->head);
		siska_spinlock_init(&bh->lock);
	}
}

int siska_get_free_pages(siska_page_t** pages, int nb_pages)
{
	siska_page_t* pg;
	siska_page_t* pg2;
	int i;
	int j;

	for (i = siska_used_pages; i + nb_pages < siska_total_pages; i += nb_pages) {

		pg = siska_pages + i;

		unsigned long flags;
		siska_irqsave(flags);

		for (j = 0; j < nb_pages; j++) {
			pg2 = pg + j;

			siska_spin_lock(&pg2->lock);
			if (pg2->refs > 0) {
				siska_spin_unlock(&pg2->lock);
				break;
			}

			pg2->refs++;
		}

		if (j < nb_pages) {
			for (--j; j >= 0; j--) {
				pg2 = pg + j;
				pg2->refs--;
				siska_spin_unlock(&pg2->lock);
			}
			siska_irqrestore(flags);

		} else {
			for (--j; j >= 0; j--) {
				pg2 = pg + j;
				siska_spin_unlock(&pg2->lock);
			}
			siska_irqrestore(flags);

			*pages = pg;
			return nb_pages;
		}
	}

	return -1;
}

int siska_free_pages(siska_page_t* pages, int nb_pages)
{
	siska_page_t* pg;
	int i;

	for (i = 0; i < nb_pages; i++) {
		pg = pages + i;

		unsigned long flags;
		siska_spin_lock_irqsave(&pg->lock, flags);
		pg->refs--;
		siska_spin_unlock_irqrestore(&pg->lock, flags);
	}
	return 0;
}

void* siska_kmalloc(int size)
{
	if (size + sizeof(long) > (PG_SIZE >> 1))
		return NULL;

	siska_block_head_t* bh;
	siska_block_t* b;
	siska_block_t* b2;
	siska_list_t*  l;

	long shift;
	long i;

	shift = siska_block_need_shift(size);

	shift = shift < BLOCK_SHIFT_MIN ? BLOCK_SHIFT_MIN : shift;

	bh = &siska_block_heads[shift];

	unsigned long flags;
	siska_spin_lock_irqsave(&bh->lock, flags);
	if (!siska_list_empty(&bh->head)) {

		l = siska_list_head(&bh->head);
		b = siska_list_data(l, siska_block_t, list);

		siska_list_del(&b->list);
		siska_spin_unlock_irqrestore(&bh->lock, flags);

		b->size_used = 1u << shift;
		siska_block_set_used(b);

		return siska_block_data(b);
	}
	siska_spin_unlock_irqrestore(&bh->lock, flags);

	for (i = shift + 1; i < PG_SHIFT; i++) {
		bh = &siska_block_heads[i];

		siska_spin_lock_irqsave(&bh->lock, flags);
		if (siska_list_empty(&bh->head)) {
			siska_spin_unlock_irqrestore(&bh->lock, flags);
			continue;
		}

		l = siska_list_head(&bh->head);
		b = siska_list_data(l, siska_block_t, list);

		siska_list_del(&b->list);
		siska_spin_unlock_irqrestore(&bh->lock, flags);

		long j;
		for (j = i - 1; j >= shift; j--) {
			bh = &siska_block_heads[j];

			b2 = (siska_block_t*)((unsigned long)b + (1u << j));
			b->size_free = 1u << j;

			siska_spin_lock_irqsave(&bh->lock, flags);
			siska_list_add_front(&bh->head, &b->list);
			siska_spin_unlock_irqrestore(&bh->lock, flags);
			b = b2;
		}

		b->size_used = 1u << shift;
		siska_block_set_used(b);

		return siska_block_data(b);
	}

	siska_page_t* pg = NULL;
	if (siska_get_free_pages(&pg, 1) < 0)
		return NULL;

	b = (siska_block_t*) siska_page_addr(pg);

//	printf("%s(), %d, b: %p, size: %ld, shift: %ld, pg: %p\n", __func__, __LINE__, b, size, shift, pg);

	for (i = PG_SHIFT - 1; i >= shift; i--) {
		bh = &siska_block_heads[i];

		b2 = (siska_block_t*)((unsigned long)b + (1u << i));

//	printf("%s(), %d, b2: %p, size: %ld, shift: %ld\n", __func__, __LINE__, b2, 1u << i, i);
		b->size_free = 1u << i;

		siska_spin_lock_irqsave(&bh->lock, flags);
		siska_list_add_front(&bh->head, &b->list);
		siska_spin_unlock_irqrestore(&bh->lock, flags);
		b = b2;
	}

	b->size_used = 1u << shift;
	siska_block_set_used(b);

	return siska_block_data(b);
}

void siska_memset(void* dst, unsigned long data, unsigned long size)
{
	asm volatile(
		"movl %0, %%ecx\n\t"
		"movl %1, %%edi\n\t"
		"movl %2, %%eax\n\t"
		"cld\n\t"
		"rep  stosb\n\t"
		:
		:"r"(size), "r"(dst), "r"(data)
		:"ecx", "edi", "eax"
	);
}

void siska_memcpy(void* dst, const void* src, unsigned long size)
{
	asm volatile(
		"movl %0, %%ecx\n\t"
		"movl %1, %%edi\n\t"
		"movl %2, %%esi\n\t"
		"cld\n\t"
		"rep  movsb\n\t"
		:
		:"r"(size), "r"(dst), "r"(src)
		:"ecx", "edi", "esi"
	);
}

void* siska_krealloc(void* p, int size)
{
	if (size <= 0) {
		siska_kfree(p);
		return NULL;
	}

	if (size + sizeof(long) > (PG_SIZE >> 1))
		return NULL;

	siska_block_t* b  = siska_block_head(p);
//	printf("b: %p\n", b);
	long old_datasize = siska_block_data_size(b);

	if (old_datasize < size) {
		void* p2 = siska_kmalloc(size);
		if (!p2)
			return NULL;

		siska_memcpy(p2, p, size);
		siska_kfree(p);
		return p2;

	} else if (old_datasize == size)
		return p;

	long old_size  = siska_block_size(b);
	long old_shift = siska_block_get_shift(old_size);
	long new_shift = siska_block_need_shift(size);

//	printf("%s(), %d, b: %p, size: %ld, shift: %ld\n", __func__, __LINE__, b, size, old_shift);

	long i;
	for (i = old_shift - 1; i >= new_shift; i--) {

		siska_block_head_t* bh;
		siska_block_t* b2;

		bh = &siska_block_heads[i];

		b2 = (siska_block_t*)((unsigned long)b + (1u << i));
		b2->size_free = 1u << i;

//		printf("%s(), %d, b2: %p, shift i: %ld\n", __func__, __LINE__, b, i);

		unsigned long flags;
		siska_spin_lock_irqsave(&bh->lock, flags);
		siska_list_add_front(&bh->head, &b2->list);
		siska_spin_unlock_irqrestore(&bh->lock, flags);
	}

	b->size_used = 1u << new_shift;
	siska_block_set_used(b);
	return p;
}

void siska_kfree(void* p)
{
	if (!p)
		return;

	siska_block_head_t* bh;
	siska_block_t* b;
	siska_block_t* b2;

	b = siska_block_head(p);

	long size  = siska_block_size(b);
	long shift = siska_block_get_shift(size);

	b->size_free = size;
//	printf("%s(), %d, b: %p, size: %ld, shift: %ld\n", __func__, __LINE__, b, size, shift);

	long i;
	for (i = shift; i < PG_SHIFT; i++) {

		bh = &siska_block_heads[i];

//	printf("%s(), %d, b: %p, size: %ld, shift i: %ld\n", __func__, __LINE__, b, size, i);

		if (!((unsigned long)b & ((size << 1) - 1))) {

			b2 = (siska_block_t*)((unsigned long)b + size);

			unsigned long flags;
			siska_spin_lock_irqsave(&bh->lock, flags);
			if (siska_block_is_used(b2)) {

				siska_list_add_front(&bh->head, &b->list);
				siska_spin_unlock_irqrestore(&bh->lock, flags);
				return;
			}
			siska_list_del(&b2->list);
			siska_spin_unlock_irqrestore(&bh->lock, flags);

			if (b2->size_free != size) {
				while (1);
			}

			size <<= 1;
			b->size_free = size;
			continue;
		}

		b2 = (siska_block_t*)((unsigned long)b - size);
//	printf("%s(), %d, b2: %p, b: %p, size: %ld, i: %ld\n", __func__, __LINE__, b2, b, size, i);

		unsigned long flags;
		siska_spin_lock_irqsave(&bh->lock, flags);
		if (siska_block_is_used(b2)) {

			siska_list_add_front(&bh->head, &b->list);
			siska_spin_unlock_irqrestore(&bh->lock, flags);
			return;
		}
		siska_list_del(&b2->list);
		siska_spin_unlock_irqrestore(&bh->lock, flags);

		b = b2;
		size <<= 1;
		b->size_free = size;
	}
//	printf("%s(), %d, b: %p, size: %ld, shift i: %ld\n", __func__, __LINE__, b, size, i);

	if (b->size_free != PG_SIZE) {
		while (1);
	}

	siska_page_t* pg = siska_addr_page((unsigned long)b);

//	printf("%s(), %d, b: %p, size: %ld, pg: %p\n", __func__, __LINE__, b, size, pg);

	siska_free_pages(pg, 1);
}

void _do_page_fault(unsigned long vaddr, unsigned long error_code, unsigned long CPL)
{
	unsigned long* pg_dir;
	unsigned long* pg_table;
	unsigned long  paddr;
	unsigned long  paddr2;
	siska_page_t*  pg  = NULL;
	siska_page_t*  pg2 = NULL;

	pg_dir = (unsigned long*) current->cr3;

	if (!(pg_dir[vaddr >> 22] & PG_FLAG_PRESENT)) {

		if (siska_get_free_pages(&pg, 1) < 0)
			return;

		paddr    = siska_page_addr(pg);

		pg_table = (unsigned long*)paddr;

		pg_dir[vaddr >> 22] = paddr | 0x7;

		siska_memset(pg_table, 0, PG_SIZE);
	} else {
		paddr = pg_dir[vaddr >> 22] & ~(PG_SIZE - 1);

		pg    = siska_addr_page(paddr);

		unsigned long flags;
		siska_spin_lock_irqsave(&pg->lock, flags);
		if (pg->refs > 1) {
			siska_spin_unlock_irqrestore(&pg->lock, flags);

			if (siska_get_free_pages(&pg2, 1) < 0)
				return;

			paddr2 = siska_page_addr(pg2);
			siska_memcpy((void*)paddr2, (void*)paddr, PG_SIZE);

			pg_dir[vaddr >> 22] = paddr2 | 0x7;

			siska_unref_page(pg);

			pg_table = (unsigned long*)paddr2;
		} else {
			siska_spin_unlock_irqrestore(&pg->lock, flags);

			pg_dir[vaddr >> 22] = paddr2 | 0x7;

			pg_table = (unsigned long*)paddr;

			int i;
			for (i = 0; i < 1024; i++)
				pg_table[i] &= ~PG_FLAG_WRITE;
		}
	}

	if (!(error_code & PG_FLAG_PRESENT)) {
		// page not exist

		if (siska_get_free_pages(&pg, 1) < 0)
			return;

		paddr = siska_page_addr(pg);

		pg_table[(vaddr >> 12) & 0x3ff] = paddr | 0x7;

	} else if (error_code & PG_FLAG_WRITE) {
		// page can't write, read only

		paddr = pg_table[(vaddr >> 12) & 0x3ff] & ~(PG_SIZE - 1);

		pg    = siska_addr_page(paddr);

		unsigned long flags;
		siska_spin_lock_irqsave(&pg->lock, flags);
		if (pg->refs > 1) {
			siska_spin_unlock_irqrestore(&pg->lock, flags);

			if (siska_get_free_pages(&pg2, 1) < 0)
				return;

			paddr2 = siska_page_addr(pg2);
			siska_memcpy((void*)paddr2, (void*)paddr, PG_SIZE);

			pg_table[(vaddr >> 12) & 0x3ff] = paddr2 | 0x7;

			siska_unref_page(pg);
		} else {
			siska_spin_unlock_irqrestore(&pg->lock, flags);

			pg_table[(vaddr >> 12) & 0x3ff] = paddr | 0x7;
		}
	}
}

void siska_free_memory(siska_task_t* task)
{
	unsigned long* dir = (unsigned long*)task->cr3;

	unsigned long i;
	unsigned long j;
	unsigned long paddr;
	unsigned long vaddr;

	for (i = 0; i < 1024; i++) {

		if (!(dir[i] & PG_FLAG_PRESENT))
			continue;

		unsigned long* table = (unsigned long*) (dir[i] & ~(PG_SIZE - 1));
		siska_page_t*  pg;

		for (j = 0; j < 1024; j++) {

			if (!(table[j] & PG_FLAG_PRESENT))
				continue;

			paddr = table[j] & ~(PG_SIZE - 1);
			vaddr = (i << 22) | (j << 12);

			if (vaddr >=       task->code3 & ~(PG_SIZE - 1)
					&& vaddr < task->end3  & ~(PG_SIZE - 1)) {
				table[j] &= ~PG_FLAG_WRITE;

				pg = siska_addr_page(paddr);
				siska_free_pages(pg, 1);
				pg = NULL;
			}
		}

		pg = siska_addr_page((unsigned long)table);
		siska_free_pages(pg, 1);
		pg = NULL;
	}
}

int siska_copy_memory(siska_task_t* child, siska_task_t* parent, unsigned long esp3)
{
	unsigned long* child_dir  = (unsigned long*)child->cr3;
	unsigned long* parent_dir = (unsigned long*)parent->cr3;

	unsigned long i;
	unsigned long j;

	unsigned long vaddr;
	unsigned long paddr;

	for (i = 0; i < 1024; i++) {

		child_dir[i] = parent_dir[i];

		if (!(parent_dir[i] & PG_FLAG_PRESENT))
			continue;

		unsigned long* table = (unsigned long*) (parent_dir[i] & ~(PG_SIZE - 1));

		siska_page_t*  pg    = siska_addr_page((unsigned long)table);
		siska_ref_page(pg);

		for (j = 0; j < 1024; j++) {

			if (!(table[j] & PG_FLAG_PRESENT))
				continue;

			paddr = table[j] & ~(PG_SIZE - 1);
			vaddr = (i << 22) | (j << 12);

			// share user code & data
			if (vaddr >=        parent->code3 & ~(PG_SIZE - 1)
					&& vaddr <= parent->brk3  & ~(PG_SIZE - 1)) {
				table[j] &= ~PG_FLAG_WRITE;

				pg = siska_addr_page(paddr);
				siska_ref_page(pg);
			}
		}
	}

	for (vaddr = esp3 & ~(PG_SIZE - 1); vaddr < parent->ebp3; vaddr += PG_SIZE) {

		i =  vaddr >> 22;
		j = (vaddr >> 12) & 0x3ff;

		if (!(parent_dir[i] & PG_FLAG_PRESENT))
			continue;

		unsigned long* child_table  = (unsigned long*) ( child_dir[i] & ~(PG_SIZE - 1));
		unsigned long* parent_table = (unsigned long*) (parent_dir[i] & ~(PG_SIZE - 1));

		siska_page_t*  pg  = NULL;

		if (child_table == parent_table) {

			if (siska_get_free_pages(&pg, 1) < 0) {
				siska_printk("siska_copy_memory, error0\n");
				goto _error;
			}

			child_table  = (unsigned long*)siska_page_addr(pg);
			child_dir[i] = (unsigned long )child_table | 0x7;

			siska_memcpy(child_table, parent_table, PG_SIZE);

			pg = siska_addr_page((unsigned long)parent_table);
			siska_unref_page(pg);
			pg = NULL;
		}

		if (!(parent_table[j] & PG_FLAG_PRESENT))
			continue;

		if (siska_get_free_pages(&pg, 1) < 0) {
			siska_printk("siska_copy_memory, error1\n");
			goto _error;
		}

		unsigned long child_paddr  = siska_page_addr(pg);
		unsigned long parent_paddr = parent_table[j] & ~(PG_SIZE - 1);

		child_table[j] = child_paddr | 0x7;

		siska_memcpy((void*)child_paddr, (void*)parent_paddr, PG_SIZE);

		pg = siska_addr_page(parent_paddr);
		siska_unref_page(pg);
		pg = NULL;

		parent_table[j] |= PG_FLAG_WRITE;
	}
	return 0;

_error:
	siska_free_memory(child);
	return -1;
}

#if 0
int main()
{
	siska_mm_init();

	void* p0 = siska_kmalloc(5);
	void* p1 = siska_kmalloc(17);

	printf("p0: %p\n", p0);
	printf("p1: %p\n", p1);

	void* p2 = siska_kmalloc(37);
	void* p3 = siska_kmalloc(43);

	printf("p2: %p\n", p2);
	printf("p3: %p\n", p3);

	char* str = "hello world!";
	int len = strlen(str);
	siska_memcpy(p2, str, len + 1);
	siska_memcpy(p3, str, len + 1);

	void* p4 = siska_krealloc(p3, 47);
	void* p5 = siska_krealloc(p2, 23);

	printf("p4: %p, %s\n", p4, p4);
	printf("p5: %p, %s\n", p5, p5);
	printf("\n");

	printf("p0: %p\n", p0);
	siska_kfree(p0);
	printf("\n");

	printf("p1: %p\n", p1);
	siska_kfree(p1);
	printf("\n");

	printf("p4: %p\n", p4);
	siska_kfree(p4);
	printf("\n");

	printf("p5: %p\n", p5);
	siska_kfree(p5);

	return 0;
}
#endif
