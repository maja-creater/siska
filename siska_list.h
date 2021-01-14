#ifndef SISKA_LIST_H
#define SISKA_LIST_H

#include"siska_def.h"

typedef struct siska_list_s	siska_list_t;

struct siska_list_s {
	struct siska_list_s* prev;
	struct siska_list_s* next;
};

static inline void siska_list_init(siska_list_t* h)
{
	h->prev = h;
	h->next = h;
}

static inline void siska_list_del(siska_list_t* n)
{
	n->prev->next = n->next;
	n->next->prev = n->prev;

	// only to avoid some wrong operations for these 2 invalid pointers
	n->prev = NULL;
	n->next = NULL;
}

static inline void siska_list_add_tail(siska_list_t* h, siska_list_t* n)
{
	h->prev->next = n;
	n->prev = h->prev;
	n->next = h;
	h->prev = n;
}

static inline void siska_list_add_front(siska_list_t* h, siska_list_t* n)
{
	h->next->prev = n;
	n->next = h->next;
	n->prev = h;
	h->next = n;
}

#define SISKA_LIST_INIT(h) {&h, &h}

#define siska_offsetof(type, member)      ((char*) &((type*)0)->member)

#define siska_list_data(l, type, member)  ((type*)((char*)l - siska_offsetof(type, member)))

#define siska_list_head(h)                ((h)->next)
#define siska_list_tail(h)                ((h)->prev)
#define siska_list_sentinel(h)            (h)
#define siska_list_next(l)                ((l)->next)
#define siska_list_prev(l)                ((l)->prev)
#define siska_list_empty(h)               ((h)->next == (h))

#define siska_list_clear(h, type, member, type_free) \
	do {\
		siska_list_t* l;\
		for (l = siska_list_head(h); l != siska_list_sentinel(h);) {\
			type* t = siska_list_data(l, type, member);\
			l = siska_list_next(l);\
			siska_list_del(&t->member);\
			type_free(t);\
			t = NULL;\
		}\
	} while(0)

#endif

