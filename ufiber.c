/* Copyright (c) 2013-2015, Drew Thoreson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <errno.h>

#include "ufiber.h"
#include "queue.h"

#define STACK_SIZE (8*1024*1024)

/* must be at least 1 */
#define FREE_LIST_MAX 1

enum {
	FS_DEAD = 0,
	FS_READY,
	FS_BLOCKED,
};

/* fiber TCB */
struct ufiber {
	UFIBER_CIRCLEQ_ENTRY(ufiber) chain;
	struct ufiber_waitlist blocked;
	struct ufiber_waitlist *blocked_on;
	char          *stack;
	void          *sp;
	unsigned long state;
	unsigned long flags;
	int           ref;
	void          *rv;   // return value
	void          **ptr; // pointer for join()
};

static struct ufiber_waitlist ready_queue // queue of ready fibers
	= UFIBER_CIRCLEQ_HEAD_INITIALIZER(ready_queue);
static struct ufiber_waitlist free_list   // list of free TCBs
	= UFIBER_CIRCLEQ_HEAD_INITIALIZER(free_list);
static unsigned free_count = 0;  // number of free TCBs
static unsigned fiber_count = 1; // number of active (non-dead) fibers

static struct ufiber *current;      // the running fiber
static struct ufiber *root;         // the top-level fiber
static struct ufiber *last_blocked; // last fiber to block

/* arch.S */
extern void *_ufiber_create(void *cx, void*(*start_routine)(void*), void *arg,
		void (*trampoline)(void), void (*ret)(void*));
extern void _ufiber_switch(void *save_sp, void *rest_sp);
extern void _ufiber_trampoline(void);

/* get a free TCB */
static struct ufiber *alloc_tcb(void)
{
	struct ufiber *ret;

	if (!UFIBER_CIRCLEQ_EMPTY(&free_list)) {
		free_count--;
		ret = UFIBER_CIRCLEQ_FIRST(&free_list);
		UFIBER_CIRCLEQ_REMOVE(&free_list, ret, chain);
		return ret;
	}

	if ((ret = malloc(sizeof(struct ufiber))) == NULL)
		return NULL;

	if ((ret->stack = malloc(STACK_SIZE)) == NULL)
		return NULL;

	return ret;
}

/* Release a TCB.
 *
 * NOTE: TCBs can't be freed immediately on ufiber_exit() because the exiting
 * thread still needs a stack for its final context_switch().  So instead of
 * freeing its argument, this function will evict a TCB from the free list and
 * free that instead.
 */
static void free_tcb(struct ufiber *tcb)
{
	struct ufiber *victim;

	UFIBER_CIRCLEQ_INSERT_HEAD(&free_list, tcb, chain);
	if (free_count < FREE_LIST_MAX) {
		free_count++;
		return;
	}

	victim = UFIBER_CIRCLEQ_LAST(&free_list);
	UFIBER_CIRCLEQ_REMOVE(&free_list, victim, chain);
	free(victim->stack);
	free(victim);
}

static void context_switch(struct ufiber *fiber)
{
	void *save_sp = &current->sp;

	if (fiber == current)
		return;

	current = fiber;
	_ufiber_switch(save_sp, &fiber->sp);
}

/* add 'fiber' to the ready queue */
static inline void ready(struct ufiber *fiber)
{
	fiber->state = FS_READY;
	UFIBER_CIRCLEQ_INSERT_TAIL(&ready_queue, fiber, chain);
}

/* unblock 'fiber', returning 'retval' */
static inline void wake(struct ufiber *tcb, void *retval)
{
	if (tcb->ptr != NULL)
		*tcb->ptr = retval;
	UFIBER_CIRCLEQ_REMOVE(tcb->blocked_on, tcb, chain);
	ready(tcb);
}

static inline void wake_one(struct ufiber_waitlist *list, void *retval)
{
	if (!UFIBER_CIRCLEQ_EMPTY(list))
		wake(UFIBER_CIRCLEQ_FIRST(list), retval);
}

/* wake all fibers on 'list', retunning 'val' to each */
static inline void wake_all(struct ufiber_waitlist *list, void *val)
{
	struct ufiber *pos, *n;

	UFIBER_CIRCLEQ_FOREACH_SAFE(pos, list, chain, n)
		wake(pos, val);
}

/* choose a new fiber to run, and run it */
static void schedule(void)
{
	struct ufiber *tcb;

	if (UFIBER_CIRCLEQ_EMPTY(&ready_queue))
		wake(last_blocked, (void*) EDEADLK);

	tcb = UFIBER_CIRCLEQ_FIRST(&ready_queue);
	UFIBER_CIRCLEQ_REMOVE(&ready_queue, tcb, chain);

	context_switch(tcb);
}

/* block 'fiber' on a given wait queue */
static void block(struct ufiber_waitlist *list, void **rv)
{
	current->ptr = rv;
	current->state = FS_BLOCKED;
	current->blocked_on = list;
	UFIBER_CIRCLEQ_INSERT_TAIL(list, current, chain);

	last_blocked = current;
	schedule();
}

/* API */

int ufiber_init(void)
{
	struct ufiber *tcb;

	tcb = alloc_tcb();
	tcb->state = FS_READY;
	tcb->ref = 100;
	UFIBER_CIRCLEQ_INIT(&tcb->blocked);

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
	struct ufiber *tcb;
	char *frame;

	if ((tcb = alloc_tcb()) == NULL)
		return ENOMEM;

	tcb->ref = (fiber == NULL || flags & UFIBER_DETACHED) ? 1 : 2;
	tcb->flags = flags;
	UFIBER_CIRCLEQ_INIT(&tcb->blocked);

	frame = tcb->stack + STACK_SIZE - sizeof(unsigned long);
	tcb->sp = _ufiber_create(frame, start_routine, arg,
			_ufiber_trampoline, ufiber_exit);

	ready(tcb);

	if (fiber)
		*fiber = tcb;

	fiber_count++;
	return 0;
}

int ufiber_join(ufiber_t fiber, void **retval)
{
	if (fiber == current)
		return EDEADLK;

	if (fiber->state == FS_DEAD && retval != NULL)
		*retval = fiber->rv;
	else if (fiber->state != FS_DEAD)
		block(&fiber->blocked, retval);
	ufiber_unref(fiber);
	return 0;
}

void ufiber_yield(void)
{
	ready(current);
	schedule();
}

int ufiber_yield_to(ufiber_t fiber)
{
	if (fiber->state == FS_DEAD)
		return ESRCH;
	if (fiber->state != FS_READY)
		return EAGAIN;

	ready(current);
	UFIBER_CIRCLEQ_REMOVE(&ready_queue, fiber, chain);
	context_switch(fiber);
	return 0;
}

void ufiber_exit(void *retval)
{
	if (--fiber_count == 0)
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

int ufiber_mutex_init(ufiber_mutex_t *mutex)
{
	UFIBER_CIRCLEQ_INIT(&mutex->blocked);
	mutex->count = 0;
	return 0;
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
		block(&mutex->blocked, (void**) &error);

	if (error)
		return error;

	mutex->count = 1;
	return 0;
}

int ufiber_mutex_unlock(ufiber_mutex_t *mutex)
{
	if (UFIBER_CIRCLEQ_EMPTY(&mutex->blocked))
		mutex->count = 0;
	else
		wake_one(&mutex->blocked, (void*) 0L);
	return 0;
}

int ufiber_mutex_trylock(ufiber_mutex_t *mutex)
{
	if (mutex->count)
		return EBUSY;
	return ufiber_mutex_lock(mutex);
}

int ufiber_barrier_init(ufiber_barrier_t *barrier, unsigned count)
{
	UFIBER_CIRCLEQ_INIT(&barrier->blocked);
	barrier->count = count;
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
		block(&barrier->blocked, (void**) &rv);
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

int ufiber_rwlock_init(ufiber_rwlock_t *lock)
{
	UFIBER_CIRCLEQ_INIT(&lock->rdblocked);
	UFIBER_CIRCLEQ_INIT(&lock->wrblocked);
	lock->reading = 0;
	return 0;
}

int ufiber_rwlock_destroy(ufiber_rwlock_t *lock)
{
	wake_all(&lock->rdblocked, (void*) ((unsigned long) EINVAL));
	wake_all(&lock->wrblocked, (void*) ((unsigned long) EINVAL));
	return 0;
}

int ufiber_rwlock_rdlock(ufiber_rwlock_t *lock)
{
	unsigned long error = 0;

	if (lock->reading == -1 || !UFIBER_CIRCLEQ_EMPTY(&lock->wrblocked))
		block(&lock->rdblocked, (void**) &error);

	if (error)
		return error;

	lock->reading++;
	return 0;
}

int ufiber_rwlock_wrlock(ufiber_rwlock_t *lock)
{
	unsigned long error = 0;

	if (lock->reading > 0)
		block(&lock->wrblocked, (void*) &error);

	if (error)
		return error;

	lock->reading = -1;
	return 0;
}

int ufiber_rwlock_tryrdlock(ufiber_rwlock_t *lock)
{
	if (lock->reading == -1 || !UFIBER_CIRCLEQ_EMPTY(&lock->wrblocked))
		return EBUSY;
	return ufiber_rwlock_rdlock(lock);
}

int ufiber_rwlock_trywrlock(ufiber_rwlock_t *lock)
{
	if (lock->reading > 0)
		return EBUSY;
	return ufiber_rwlock_wrlock(lock);
}

int ufiber_rwlock_unlock(ufiber_rwlock_t *lock)
{
	struct ufiber *next;

	if (lock->reading == -1) {
		if (UFIBER_CIRCLEQ_EMPTY(&lock->wrblocked)) {
			lock->reading = 0;
			wake_all(&lock->rdblocked, (void*) 0L);
		} else {
			next = UFIBER_CIRCLEQ_FIRST(&lock->wrblocked);
			UFIBER_CIRCLEQ_REMOVE(&lock->wrblocked, next, chain);
			ready(next);
		}
	} else if (--lock->reading == 0) {
		wake_one(&lock->wrblocked, (void*) 0L);
	}
	return 0;
}

int ufiber_cond_init(ufiber_cond_t *cond)
{
	UFIBER_CIRCLEQ_INIT(cond);
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

	block(cond, (void*) &error);
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
	wake_one(cond, (void*) 0L);
	return 0;
}
