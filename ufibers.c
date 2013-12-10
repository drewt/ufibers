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

#include "ufibers.h"
#include "list.h"

/* compiler */
#if __GNUC__
# if __i386__
#  define ARCH_IA32 1
# elif __amd64__
#  define ARCH_AMD64 1
# else
#  error unsupported architecture
# endif /* __i386__ */
#else
# error unsupported compiler
#endif /* GCC */

/* architecture */
#if ARCH_AMD64
#define CONTEXT_SIZE (8*16)	/* 15 registers + eflags */
#define RA_POS (16)		/* return address */
#define SR_POS (14)		/* %rax */
#define ARG_POS (9)		/* %rdi */
#elif ARCH_IA32
#define CONTEXT_SIZE (4*8)
#define RA_POS  (8)
#define SR_POS  (6)		/* %eax */
#define ARG_POS (5)		/* %ebx */
#endif

/* Allocating TCBs is expensive.  This should be large. */
#define ALLOC_STEP 32

#define STACK_SIZE (8*1024*1024)

/* adjust a pointer, e.g. to maintain consistency between realloc()s. */
#define adjust_ptr(p, diff) ((p) = (void*) ((unsigned long)(p)) + (diff))

/*
 * Fiber IDs (fids) are unsigned longs, where the first COOKIE_BITS bits are
 * used to store a "cookie" value, and the rest are an index into the TCB
 * table.  The cookie is used to prevent fibers from causing havok with stale
 * fids.
 */

#define COOKIE_BITS 2
#define FID_MASK (~0 << COOKIE_BITS)
#define COOKIE_MASK ((1 << COOKIE_BITS) - 1)

/* get index into 'fibers' table from fid */
#define fiber_index(fid) ((fid) >> COOKIE_BITS)

/* update a fid "cookie" */
#define next_fid(fid) (clean_fid(fid) | ((cookie(fid) + 1) & COOKIE_MASK))
#define clean_fid(fid) ((fid) & FID_MASK)
#define cookie(fid) ((fid) & COOKIE_MASK)

enum {
	FS_DEAD = 0,
	FS_READY,
	FS_JOINING,
};

/* fiber TCB */
struct fiber {
	struct list_head chain;		/* link for ready_queue, block lists */
	struct list_head blocked;	/* list of fibers waiting on join() */
	fiber_t fid;			/* fiber id */
	char *stack;			/* stack memory */
	unsigned long esp;		/* stack pointer */
	unsigned long state;		/* FS_READY, FS_DEAD, etc. */
	unsigned long flags;
	void **ptr;			/* pointer for join() */
};

static unsigned int nr_fibers = 0;
static struct fiber *fibers = NULL;

static LIST_HEAD(ready_queue); /* queue of ready fibers */

struct fiber *current;		/* the running fiber */
struct fiber *main_fiber;	/* the top-level fiber */

/* grows the pool of TCBs */
static void grow_fibers(void)
{
	unsigned long diff;
	unsigned int old_size = nr_fibers;
	struct fiber *old_ptr = fibers;

	nr_fibers += ALLOC_STEP;
	fibers = realloc(fibers, nr_fibers * sizeof(*fibers));

	/* adjust pointers */
	diff = ((unsigned long)fibers) - ((unsigned long) old_ptr);
	adjust_ptr(current, diff);
	for (fiber_t i = 0; i < old_size; i++) {
		adjust_ptr(fibers[i].chain.next, diff);
		adjust_ptr(fibers[i].chain.prev, diff);
		adjust_ptr(fibers[i].blocked.next, diff);
		adjust_ptr(fibers[i].blocked.prev, diff);
	}

	for (fiber_t i = old_size; i < nr_fibers; i++) {
		fibers[i].fid = i << COOKIE_BITS;
		fibers[i].state = FS_DEAD;
	}
}

/* get a free TCB */
static fiber_t alloc_tcb(void)
{
	fiber_t i;

	for (i = 0; i < nr_fibers; i++) {
		if (fibers[i].state == FS_DEAD)
			break;
	}

	if (i == nr_fibers)
		grow_fibers();

	return i;
}

/* We need to control the stack frame for this function.  To make sure gcc
 * doesn't do anything unexpected, it is written in asm.  Any additional work
 * can be performed in the wrapper below.
 */
void __context_switch(unsigned long *save_esp, unsigned long *rest_esp);
asm("\n"
"__context_switch:			\n\t"
#if ARCH_AMD64
	/* save context */
	"pushfq     			\n\t"
	"pushq %rax			\n\t"
	"pushq %rbx			\n\t"
	"pushq %rcx			\n\t"
	"pushq %rdx			\n\t"
	"pushq %rsi			\n\t"
	"pushq %rdi			\n\t"
	"pushq %r8 			\n\t"
	"pushq %r9 			\n\t"
	"pushq %r10			\n\t"
	"pushq %r11			\n\t"
	"pushq %r12			\n\t"
	"pushq %r13			\n\t"
	"pushq %r14			\n\t"
	"pushq %r15			\n\t"
	"pushq %rbp			\n\t"
	/* switch stacks */
	"movq  %rsp, (%rdi)		\n\t"
	"movq  (%rsi), %rsp		\n\t"
	/* restore context */
	"popq  %rbp			\n\t"
	"popq  %r15			\n\t"
	"popq  %r14			\n\t"
	"popq  %r13			\n\t"
	"popq  %r12			\n\t"
	"popq  %r11			\n\t"
	"popq  %r10			\n\t"
	"popq  %r9 			\n\t"
	"popq  %r8 			\n\t"
	"popq  %rdi			\n\t"
	"popq  %rsi			\n\t"
	"popq  %rdx			\n\t"
	"popq  %rcx			\n\t"
	"popq  %rbx			\n\t"
	"popq  %rax			\n\t"
	"popfq     			\n\t"
	"ret				\n\t"
#elif ARCH_IA32
	"pushfl				\n\t"
	"pushl %eax			\n\t"
	"pushl %ebx			\n\t"
	"pushl %ecx			\n\t"
	"pushl %edx			\n\t"
	"pushl %esi			\n\t"
	"pushl %edi			\n\t"
	"pushl %ebp			\n\t"
	"movl  %esp, -4(%esp)		\n\t"
	"movl  -8(%esp), %esp		\n\t"
	"popl  %ebp			\n\t"
	"popl  %edi			\n\t"
	"popl  %esi			\n\t"
	"popl  %edx			\n\t"
	"popl  %ecx			\n\t"
	"popl  %ebx			\n\t"
	"popl  %eax			\n\t"
	"popfl				\n\t"
	"ret				\n\t"
#endif
);

static void context_switch(struct fiber *fiber)
{
	unsigned long *save_esp = &current->esp;

	if (fiber == current)
		return;

	current = fiber;
	__context_switch(save_esp, &fiber->esp);
}

/* This is the trampoline to start a fiber.  It calls the fiber's start routine
 * and then exits.
 *
 * The trampoline is entered from the 'ret' in __context_switch().
 * fiber_create() sets up the stack so that the fiber's start routine and its
 * argument are in registers when this occurs.
 */
void trampoline(void);
asm("\n"
"trampoline:			\n\t"
#if ARCH_AMD64
	"call *%rax		\n\t"
	"movq %rax, %rdi	\n\t"
	"call fiber_exit	\n\t"
#elif ARCH_IA32
	"pushl %ebx		\n\t"
	"call  *%eax		\n\t"
	"pushl %eax		\n\t"
	"call fiber_exit	\n\t"
#endif
);

/* add 'tcb' to the ready queue */
static inline void ready(struct fiber *tcb)
{
	tcb->state = FS_READY;
	list_add_tail(&tcb->chain, &ready_queue);
}

/* choose a new fiber to run */
static void schedule(void)
{
	struct fiber *tcb;

	if (list_empty(&ready_queue)) {
		fprintf(stderr, "error: deadlock detected\n");
		exit(EXIT_FAILURE);
	}

	tcb = list_entry(ready_queue.next, struct fiber, chain);
	list_del(ready_queue.next);

	context_switch(tcb);
}

/* API */

int fiber_init(void)
{
	fiber_t fid;
	struct fiber *tcb;

	fid = alloc_tcb();
	tcb = &fibers[fid];
	tcb->state = FS_READY;
	INIT_LIST_HEAD(&tcb->blocked);

	main_fiber = current = tcb;
	return 0;
}

fiber_t fiber_self(void)
{
	return current->fid;
}

int fiber_create(fiber_t *fiber, unsigned long flags,
		void *(*start_routine)(void*), void *arg)
{
	fiber_t fid;
	struct fiber *tcb;
	unsigned long *frame;

	fid = alloc_tcb();
	tcb = &fibers[fid];

	tcb->fid = next_fid(tcb->fid);
	tcb->flags = flags;
	tcb->state = FS_READY;
	INIT_LIST_HEAD(&tcb->blocked);

	tcb->stack = malloc(STACK_SIZE);
	frame = (unsigned long*) (tcb->stack + STACK_SIZE - CONTEXT_SIZE - 128);
	tcb->esp = (unsigned long) frame;

	frame[ARG_POS] = (unsigned long) arg;
	frame[SR_POS]  = (unsigned long) start_routine;
	frame[RA_POS]  = (unsigned long) trampoline;

	list_add_tail(&tcb->chain, &ready_queue);

	*fiber = tcb->fid;

	return 0;
}

int fiber_join(fiber_t fiber, void **retval)
{
	unsigned long index = fiber_index(fiber);
	struct fiber *tcb = &fibers[index];

	if (index >= nr_fibers)
		return -1;

	if (tcb->fid != fiber || tcb->state == FS_DEAD)
		return -1; /* stale fid */

	current->state = FS_JOINING;
	current->ptr = retval;
	list_add_tail(&current->chain, &tcb->blocked);

	schedule();
	return 0;
}

void fiber_yeild(void)
{
	ready(current);
	schedule();
}

int fiber_yeild_to(fiber_t fiber)
{
	unsigned long index = fiber_index(fiber);
	struct fiber *tcb = &fibers[index];

	if (index >= nr_fibers)
		return -1;

	if (tcb->fid != fiber || tcb->state != FS_READY)
		return -1;

	ready(current);
	context_switch(tcb);
	return 0;
}

void fiber_exit(void *retval)
{
	struct fiber *pos, *n;

	if (current == main_fiber)
		exit(((long)retval));

	current->state = FS_DEAD;

	list_for_each_entry_safe(pos, n, &current->blocked, chain) {
		pos->state = FS_READY;
		if (pos->ptr != NULL)
			*pos->ptr = retval;
		list_del(&pos->chain);
		list_add_tail(&pos->chain, &ready_queue);
	}

	schedule();
}
