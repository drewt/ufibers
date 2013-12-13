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

#define BUF_LEN 8
#define NR_FIBERS 16

#define next(pos) (((pos) + 1) % BUF_LEN)

static unsigned long shared[BUF_LEN];
static size_t wpos = 0;
static size_t rpos = 0;

static int pc(void *(*producer)(void*), void *(*consumer)(void*))
{
	ufiber_t fids[NR_FIBERS * 2];

	wpos = rpos = 0;

	for (unsigned long i = 0; i < NR_FIBERS; i++)
		ufiber_create(&fids[i], 0, producer, (void*) i);
	for (int i = 0; i < NR_FIBERS; i++)
		ufiber_create(&fids[i + NR_FIBERS], 0, consumer, NULL);

	for (int i = 0; i < NR_FIBERS * 2; i++) {
		ufiber_join(fids[i], NULL);
		ufiber_unref(fids[i]);
	}
	return 0; // TODO: collect and check values
}

static void *yeild_producer(void *data)
{
	if ((unsigned long)data % 2 == 0)
		ufiber_yeild();

	while (next(wpos) == rpos)
		ufiber_yeild();

	shared[wpos] = (unsigned long) data;
	wpos = next(wpos);

	return NULL;
}

static void *yeild_consumer(void *data)
{
	while (rpos == wpos)
		ufiber_yeild();

	printf("read %lu\n", shared[rpos]);
	rpos = next(rpos);

	return NULL;
}

static int yeild_test(void)
{
	return pc(yeild_producer, yeild_consumer);
}

static ufiber_mutex_t mutex;

static void *mutex_thread(void *data)
{
	ufiber_mutex_lock(&mutex);
	ufiber_yeild();
	printf("read %lu\n", *shared);
	ufiber_yeild();
	*shared = (unsigned long) data;
	ufiber_yeild();
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

	for (int i = 0; i < NR_FIBERS; i++) {
		ufiber_join(fids[i], NULL);
		ufiber_unref(fids[i]);
	}

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
		ufiber_yeild();

	if (*shared != 0)
		rv = -1;

	ufiber_create(&fids[NR_FIBERS-1], 0, barrier_thread, NULL);

	for (int i = 0; i < NR_FIBERS; i++) {
		ufiber_join(fids[i], NULL);
		ufiber_unref(fids[i]);
	}

	if (*shared != NR_FIBERS)
		rv = -2;

	ufiber_barrier_destroy(&barrier);
	return rv;
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
	run_test("yeild", yeild_test);
	run_test("mutex", mutex_test);
	run_test("barrier", barrier_test);
}
