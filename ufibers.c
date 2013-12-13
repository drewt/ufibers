/* Copyright 2013 Drew Thoreson
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#include "list.h"
#include "ufibers.h"

/* architecture */
#if ARCH_AMD64
#define CONTEXT_SIZE (8*7) /* 6 registers + eflags */
#define RA_POS  (7)        /* return address */
#define SR_POS  (2)        /* %r14 */
#define ARG_POS (1)        /* %r15 */
#elif ARCH_IA32
#define CONTEXT_SIZE (4*8)
#define RA_POS  (8)
#define SR_POS  (6)         /* %eax */
#define ARG_POS (5)         /* %ebx */
#endif

/* must be at least 1 */
#define FREE_LIST_MAX 1

#define dequeue(q) list_remove_head(q, struct fiber, chain)

enum {
	FS_DEAD = 0,
	FS_READY,
	FS_BLOCKED,
};

/* fiber TCB */
struct fiber {
	struct        list_head chain;   /* link for ready_queue, block lists */
	struct        list_head blocked; /* list of fibers waiting on join() */
	char          *stack;            /* stack memory */
	unsigned long esp;               /* stack pointer */
	unsigned long state;             /* FS_READY, FS_DEAD, etc. */
	unsigned long flags;
	int           ref;
	void          *rv;               /* return value */
	void          **ptr;             /* pointer for join() */
};

static LIST_HEAD(ready_queue);  /* queue of ready fibers */
static LIST_HEAD(free_list);    /* list of free TCBs */
static unsigned free_count = 0; /* number of free TCBs */

static struct fiber *current;   /* the running fiber */
static struct fiber *root;      /* the top-level fiber */

/* arch.S */
extern void __context_switch(unsigned long *save_esp, unsigned long *rest_esp);
extern void trampoline(void);

/* get a free TCB */
static struct fiber *alloc_tcb(void)
{
	if (!list_empty(&free_list)) {
		free_count--;
		return list_remove_head(&free_list, struct fiber, chain);
	}
	return malloc(sizeof(struct fiber));
}

/* Release a TCB.
 *
 * NOTE: TCBs can't be freed immediately on ufiber_exit() because the exiting
 * thread still needs a stack for its final context_switch().  So instead of
 * freeing its argument, this function will evict a TCB from the free list and
 * free that instead.
 */
static void free_tcb(struct fiber *tcb)
{
	struct fiber *victim;

	list_add(&tcb->chain, &free_list);
	if (free_count < FREE_LIST_MAX) {
		free_count++;
		return;
	}

	victim = list_entry(free_list.prev, struct fiber, chain);
	list_del(&victim->chain);
	free(victim->stack);
	free(victim);
}

static void context_switch(struct fiber *fiber)
{
	unsigned long *save_esp = &current->esp;

	if (fiber == current)
		return;

	current = fiber;
	__context_switch(save_esp, &fiber->esp);
}

/* choose a new fiber to run, and run it */
static void schedule(void)
{
	struct fiber *tcb;

	if (list_empty(&ready_queue)) {
		fprintf(stderr, "error: deadlock detected\n");
		exit(EXIT_FAILURE);
	}

	tcb = list_remove_head(&ready_queue, struct fiber, chain);

	context_switch(tcb);
}

/* block 'fiber' on a given wait queue */
static inline void block(struct fiber *fiber, struct list_head *list,
		void **rv)
{
	fiber->ptr = rv;
	fiber->state = FS_BLOCKED;
	list_add_tail(&fiber->chain, list);
	schedule();
}

/* add 'fiber' to the ready queue */
static inline void ready(struct fiber *fiber)
{
	fiber->state = FS_READY;
	list_add_tail(&fiber->chain, &ready_queue);
}

/* unblock 'fiber', returning 'retval' */
static inline void wake(struct fiber *tcb, void *retval)
{
	if (tcb->ptr != NULL)
		*tcb->ptr = retval;
	list_del(&tcb->chain);
	ready(tcb);
}

/* wake all fibers on 'list', retunning 'val' to each */
static inline void wake_all(struct list_head *list, void *val)
{
	struct fiber *pos, *n;

	list_for_each_entry_safe(pos, n, list, chain)
		wake(pos, val);
}

/* API */

int ufiber_init(void)
{
	struct fiber *tcb;

	tcb = alloc_tcb();
	tcb->state = FS_READY;
	tcb->ref = 100;
	INIT_LIST_HEAD(&tcb->blocked);

	root = current = tcb;
	return 0;
}

ufiber_t ufiber_self(void)
{
	return current;
}

int ufiber_create(ufiber_t *fiber, unsigned long flags,
		void *(*start_routine)(void*), void *arg)
{
	struct fiber *tcb;
	unsigned long *frame;

	tcb = alloc_tcb();
	tcb->stack = malloc(STACK_SIZE);
	tcb->ref = (fiber == NULL || flags & FF_NOREF) ? 1 : 2;
	tcb->flags = flags;
	INIT_LIST_HEAD(&tcb->blocked);

	frame = (unsigned long*) (tcb->stack + STACK_SIZE - CONTEXT_SIZE - 128);
	tcb->esp = (unsigned long) frame;

	frame[ARG_POS] = (unsigned long) arg;
	frame[SR_POS]  = (unsigned long) start_routine;
	frame[RA_POS]  = (unsigned long) trampoline;

	ready(tcb);

	if (fiber != NULL)
		*fiber = tcb;

	return 0;
}

int ufiber_join(ufiber_t fiber, void **retval)
{
	if (fiber->state == FS_DEAD && retval != NULL)
		*retval = fiber->rv;
	else if (fiber->state != FS_DEAD)
		block(current, &fiber->blocked, retval);
	return 0;
}

void ufiber_yeild(void)
{
	ready(current);
	schedule();
}

int ufiber_yeild_to(ufiber_t fiber)
{
	if (fiber->state != FS_READY)
		return -1;

	ready(current);
	list_del(&fiber->chain);
	context_switch(fiber);
	return 0;
}

void ufiber_exit(void *retval)
{
	if (current == root)
		exit(((long)retval));

	current->rv = retval;
	current->state = FS_DEAD;
	wake_all(&current->blocked, retval);
	ufiber_unref(current);

	schedule();
}

void ufiber_ref(ufiber_t fiber)
{
	fiber->ref++;
}

void ufiber_unref(ufiber_t fiber)
{
	if (--fiber->ref == 0)
		free_tcb(fiber);
}

int ufiber_mutex_destroy(ufiber_mutex_t *mutex)
{
	wake_all(&mutex->blocked, (void*) -1L);
	return 0;
}

int ufiber_mutex_lock(ufiber_mutex_t *mutex)
{
	unsigned long error = 0;

	if (mutex->count)
		block(current, &mutex->blocked, (void**) &error);

	if (error)
		return error;

	mutex->count = 1;
	return 0;
}

int ufiber_mutex_unlock(ufiber_mutex_t *mutex)
{
	struct fiber *next;

	if (list_empty(&mutex->blocked)) {
		mutex->count = 0;
		return 0;
	}

	next = list_remove_head(&mutex->blocked, struct fiber, chain);
	ready(next);
	return 0;
}

int ufiber_barrier_destroy(ufiber_barrier_t *barrier)
{
	wake_all(&barrier->blocked, (void*) ((unsigned long) EINVAL));
	return 0;
}

int ufiber_barrier_wait(ufiber_barrier_t *barrier)
{
	unsigned long rv = 0;

	if (--barrier->count == 0) {
		wake_all(&barrier->blocked, (void*) 0L);
		rv = UFIBER_BARRIER_SERIAL_FIBER;
	} else {
		block(current, &barrier->blocked, (void**) &rv);
	}

	return rv;
}

/*
 * rwlocks
 *
 * A value of -1 for lock->reading means that it is currently write-locked.
 * When unlocking a write-locked rwlock, queued writers are unblocked first.
 * Only when there are no writers left are any queued readers unblocked.
 */

int ufiber_rwlock_destroy(ufiber_rwlock_t *lock)
{
	wake_all(&lock->rdblocked, (void*) ((unsigned long) EINVAL));
	wake_all(&lock->wrblocked, (void*) ((unsigned long) EINVAL));
	return 0;
}

int ufiber_rwlock_rdlock(ufiber_rwlock_t *lock)
{
	unsigned long error = 0;

	if (lock->reading == -1 || !list_empty(&lock->wrblocked))
		block(current, &lock->rdblocked, (void**) &error);

	if (error)
		return error;

	lock->reading++;
	return 0;
}

int ufiber_rwlock_wrlock(ufiber_rwlock_t *lock)
{
	unsigned long error = 0;

	if (lock->reading > 0)
		block(current, &lock->wrblocked, (void*) &error);

	if (error)
		return error;

	lock->reading = -1;
	return 0;
}

int ufiber_rwlock_unlock(ufiber_rwlock_t *lock)
{
	struct fiber *next;

	if (lock->reading == -1) {
		if (list_empty(&lock->wrblocked)) {
			lock->reading = 0;
			wake_all(&lock->rdblocked, (void*) 0L);
		} else {
			next = dequeue(&lock->wrblocked);
			ready(next);
		}
	} else if (--lock->reading == 0 && !list_empty(&lock->wrblocked)) {
		next = dequeue(&lock->wrblocked);
		ready(next);
	}
	return 0;
}

int ufiber_cond_destroy(ufiber_cond_t *cond)
{
	return 0;
}

int ufiber_cond_wait(ufiber_cond_t *cond, ufiber_mutex_t *mutex)
{
	unsigned long error = 0;

	if (mutex != NULL && (error = ufiber_mutex_unlock(mutex) != 0))
		return error;

	block(current, cond, (void*) &error);
	if (error)
		return error;

	if (mutex != NULL)
		error = ufiber_mutex_unlock(mutex);

	return error;
}

int ufiber_cond_broadcast(ufiber_cond_t *cond)
{
	wake_all(cond, (void*) 0L);
	return 0;
}

int ufiber_cond_signal(ufiber_cond_t *cond)
{
	if (!list_empty(cond))
		ready(dequeue(cond));
	return 0;
}
