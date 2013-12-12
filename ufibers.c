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

enum {
	FS_DEAD = 0,
	FS_READY,
	FS_JOINING,
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

/* add 'tcb' to the ready queue */
static inline void ready(struct fiber *tcb)
{
	tcb->state = FS_READY;
	list_add_tail(&tcb->chain, &ready_queue);
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
	if (fiber->state == FS_DEAD)
		return 0;

	current->state = FS_JOINING;
	current->ptr = retval;
	list_add_tail(&current->chain, &fiber->blocked);

	schedule();
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
	context_switch(fiber);
	return 0;
}

void ufiber_exit(void *retval)
{
	struct fiber *pos, *n;

	if (current == root)
		exit(((long)retval));

	current->state = FS_DEAD;

	list_for_each_entry_safe(pos, n, &current->blocked, chain) {
		if (pos->ptr != NULL)
			*pos->ptr = retval;
		list_del(&pos->chain);
		ready(pos);
	}

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
