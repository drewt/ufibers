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

#include "ufiber.h"

#define BUF_LEN 8
#define NR_FIBERS 16

#define next(pos) (((pos) + 1) % BUF_LEN)

static unsigned long shared[BUF_LEN];
static size_t wpos = 0;
static size_t rpos = 0;

static inline void yield_n(unsigned n)
{
	for (unsigned i = 0; i < n; i++)
		ufiber_yield();
}

static inline void yield_to_n(ufiber_t to, unsigned n)
{
	for (unsigned i = 0; i < n; i++)
		ufiber_yield_to(to);
}

static int pc(void *(*producer)(void*), void *(*consumer)(void*))
{
	ufiber_t fids[NR_FIBERS * 2];

	wpos = rpos = 0;

	for (unsigned long i = 0; i < NR_FIBERS; i++)
		ufiber_create(&fids[i], 0, producer, (void*) i);
	for (int i = 0; i < NR_FIBERS; i++)
		ufiber_create(&fids[i + NR_FIBERS], 0, consumer, NULL);

	for (int i = 0; i < NR_FIBERS * 2; i++)
		ufiber_join(fids[i], NULL);
	return 0; // TODO: collect and check values
}

static void *yield_producer(void *data)
{
	if ((unsigned long)data % 2 == 0)
		ufiber_yield();

	while (next(wpos) == rpos)
		ufiber_yield();

	shared[wpos] = (unsigned long) data;
	wpos = next(wpos);

	return NULL;
}

static void *yield_consumer(void *data)
{
	while (rpos == wpos)
		ufiber_yield();

	printf("read %lu\n", shared[rpos]);
	rpos = next(rpos);

	return NULL;
}

static int yield_test(void)
{
	return pc(yield_producer, yield_consumer);
}

static ufiber_mutex_t mutex;

static void *mutex_thread(void *data)
{
	ufiber_mutex_lock(&mutex);
	ufiber_yield();
	printf("read %lu\n", *shared);
	ufiber_yield();
	*shared = (unsigned long) data;
	ufiber_yield();
	ufiber_mutex_unlock(&mutex);
	return NULL;
}

static int mutex_test(void)
{
	ufiber_t fids[NR_FIBERS];

	*shared = 0;
	ufiber_mutex_init(&mutex);

	for (unsigned long i = 0; i < NR_FIBERS; i++)
		ufiber_create(&fids[i], 0, mutex_thread, (void*) (i+1));

	for (int i = 0; i < NR_FIBERS; i++)
		ufiber_join(fids[i], NULL);

	ufiber_mutex_destroy(&mutex);
	return 0; // TODO: collect and check values
}

static ufiber_barrier_t barrier;

static void *barrier_thread(void *data)
{
	ufiber_barrier_wait(&barrier);
	shared[0]++;
	return NULL;
}

static int barrier_test(void)
{
	ufiber_t fids[NR_FIBERS];
	int rv = 0;

	*shared = 0;
	ufiber_barrier_init(&barrier, NR_FIBERS);

	for (int i = 0; i < NR_FIBERS-1; i++)
		ufiber_create(&fids[i], 0, barrier_thread, NULL);

	for (int i = 0; i < NR_FIBERS-1; i++)
		ufiber_yield_to(fids[i]);

	if (*shared != 0)
		rv = -1;

	ufiber_create(&fids[NR_FIBERS-1], 0, barrier_thread, NULL);

	for (int i = 0; i < NR_FIBERS; i++)
		ufiber_join(fids[i], NULL);

	if (*shared != NR_FIBERS)
		rv = -2;

	ufiber_barrier_destroy(&barrier);
	return rv;
}

static ufiber_rwlock_t rwlock;
static ufiber_t writer;

static void *read_thread(void *data)
{
	unsigned long rv;
	ufiber_rwlock_rdlock(&rwlock);
	/* make sure the writer blocks before unlocking */
	yield_to_n(writer, NR_FIBERS-1);
	rv = *shared;
	ufiber_rwlock_unlock(&rwlock);
	return (void*) rv;
}

static void *write_thread(void *data)
{
	/* allow all readers to acquire the lock */
	yield_n(NR_FIBERS-1);
	ufiber_rwlock_wrlock(&rwlock);
	*shared = (unsigned long) data;
	ufiber_rwlock_unlock(&rwlock);
	return NULL;
}

static int rwlock_test(void)
{
	ufiber_t readers[NR_FIBERS-1];
	unsigned long v = 0, acc = 0;

	*shared = 0;
	ufiber_rwlock_init(&rwlock);

	ufiber_create(&writer, 0, write_thread, (void*) 1L);
	for (int i = 0; i < NR_FIBERS-1; i++)
		ufiber_create(&readers[i], 0, read_thread, NULL);

	for (int i = 0; i < NR_FIBERS-1; i++) {
		ufiber_join(readers[i], (void**) &v);
		acc += v;
	}
	ufiber_join(writer, NULL);

	if (acc != 0)
		return -1;

	return 0;
}

ufiber_cond_t cond;

static void *wait_thread(void *data)
{
	unsigned long rv;
	ufiber_cond_wait(&cond, NULL);
	rv = *shared;
	ufiber_exit((void*)rv);
	return (void*) rv;
}

static void *wake_thread(void *data)
{
	yield_n(NR_FIBERS-1);

	*shared = 1;
	ufiber_cond_signal(&cond);

	yield_n(NR_FIBERS-1);

	*shared = 0;
	ufiber_cond_broadcast(&cond);

	return NULL;
}

static int cond_test(void)
{
	ufiber_t fids[NR_FIBERS];
	unsigned long v = 0, acc = 0;

	*shared = 0;
	ufiber_cond_init(&cond);

	ufiber_create(&fids[0], 0, wake_thread, NULL);
	for (int i = 1; i < NR_FIBERS; i++)
		ufiber_create(&fids[i], 0, wait_thread, NULL);

	ufiber_join(fids[0], NULL);
	for (int i = 1; i < NR_FIBERS; i++) {
		ufiber_join(fids[i], (void**) &v);
		acc += v;
	}

	if (acc != 1)
		return -1;
	return 0;
}

static void *hold_fib(void *data)
{
	ufiber_mutex_init(&mutex);
	ufiber_mutex_lock(&mutex);

	ufiber_yield();

	return NULL;
}

static int deadlock_test(void)
{
	ufiber_t hold;
	int rc = 0;

	ufiber_create(&hold, 0, hold_fib, (void*)ufiber_self());
	ufiber_yield();

	if (ufiber_mutex_lock(&mutex) != EDEADLK)
		rc = 1;

	ufiber_join(hold, NULL);

	return rc;
}

static void *new_main(void *data)
{
	printf("STATUS: 0 SUCCESS\n");
	return NULL;
}

static int exit_test(void)
{
	ufiber_create(NULL, 0, new_main, NULL);
	ufiber_exit(NULL);
	return 1;
}

static void run_test(char *name, int (*test)(void))
{
	int status;

	printf("TEST: %s\n", name);
	status = test();
	printf("STATUS: %d %s\n\n", status, status ? "FAIL" : "SUCCESS");
}

int main(void)
{
	ufiber_init();
	run_test("yield", yield_test);
	run_test("mutex", mutex_test);
	run_test("barrier", barrier_test);
	run_test("rwlock", rwlock_test);
	run_test("cond", cond_test);
	run_test("deadlock", deadlock_test);
	run_test("exit", exit_test);
	printf("ERROR: unreachable code!\n");
}
