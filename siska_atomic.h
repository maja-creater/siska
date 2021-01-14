#ifndef SISKA_ATOMIC_H
#define SISKA_ATOMIC_H

typedef struct {
	volatile int refs;
} siska_atomic_t;

typedef struct {
	siska_atomic_t v;
} siska_spinlock_t;

#define LOCK "lock;"


static inline int siska_atomic_xchg(siska_atomic_t* v, int i)
{
	asm volatile("xchg %0, %1"
			:"=m"(v->refs), "=r"(i)
			:"1"(i)
			:
	);
	return i;
}

static inline void siska_atomic_inc(siska_atomic_t* v)
{
	asm volatile(LOCK"incl %0":"=m"(v->refs) ::);
}

static inline void siska_atomic_dec(siska_atomic_t* v)
{
	asm volatile(LOCK"decl %0":"=m"(v->refs) ::);
}

static inline int siska_atomic_dec_and_test(siska_atomic_t* v)
{
	char ret;
	asm volatile(LOCK"decl %0; setz %1":"=m"(v->refs), "=r"(ret)::);
	return ret;
}

static inline void siska_cli()
{
	asm volatile("cli":::);
}
static inline void siska_sti()
{
	asm volatile("sti":::);
}

#define SISKA_SPINLOCK_INIT {0}

static inline void siska_spinlock_init(siska_spinlock_t* lock)
{
	lock->v.refs = 0;
}

#define siska_irqsave(flags) \
	do { \
		asm volatile( \
			"pushfl\n\t" \
			"popl %0\n\t" \
			:"=r"(flags) \
			: \
			: \
		); \
		siska_cli(); \
	} while (0)

#define siska_irqrestore(flags) \
	do { \
		asm volatile( \
			"pushl %0\n\t" \
			"popfl\n\t" \
			: \
			:"r"(flags) \
			:"flags" \
		); \
	} while (0)

#undef SISKA_SMP

#ifdef SISKA_SMP
#define siska_spin_lock_irqsave(lock, flags) \
	do { \
		siska_irqsave(flags); \
		while(0 != siska_atomic_xchg(&lock->v, 1)); \
	} while (0)

#define siska_spin_unlock_irqrestore(lock, flags) \
	do { \
		lock->v.refs = 0; \
		siska_irqrestore(flags); \
	} while (0)

#define siska_spin_lock(lock) \
	do { \
		while(0 != siska_atomic_xchg(&lock->v, 1)); \
	} while (0)

#define siska_spin_unlock(lock) \
	do { \
		lock->v.refs = 0; \
	} while (0)

#else
#define siska_spin_lock_irqsave(lock, flags)      siska_irqsave(flags)
#define siska_spin_unlock_irqrestore(lock, flags) siska_irqrestore(flags)
#define siska_spin_lock(lock) 
#define siska_spin_unlock(lock) 
#endif

#endif

