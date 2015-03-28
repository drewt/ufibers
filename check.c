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
#include <check.h>
#include "ufiber.h"

#define NR_FIBERS 30

#ifndef ck_assert_ptr_eq
#define ck_assert_ptr_eq(a, b) ck_assert((void*)a == (void*)b)
#endif

static int counter;
static ufiber_t other;
static ufiber_mutex_t mutex;
static ufiber_barrier_t barrier;
static ufiber_rwlock_t rwlock;
static ufiber_cond_t cond;

static inline void ck_ufiber_create(ufiber_t *fiber, unsigned long flags,
		void *(*start_routine)(void*), void *arg)
{
	if (ufiber_create(fiber, flags, start_routine, arg))
		ck_abort_msg("ufiber_create() failed");
}

static inline void ck_ufiber_join(ufiber_t fiber, void **retval)
{
	if (ufiber_join(fiber, retval))
		ck_abort_msg("ufiber_join() failed");
}

static void *uf_create(void *data)
{
	ck_assert_ptr_eq(data, &counter);
	*((int*)data) = 1;
	return &counter;
}

START_TEST(test_ufiber_create)
{
	ufiber_t fid;
	void *retval;

	counter = 0;
	ck_ufiber_create(&fid, 0, uf_create, &counter);
	ck_ufiber_join(fid, &retval);
	ck_assert_int_ne(counter, 0);
	ck_assert_ptr_eq(retval, &counter);
}
END_TEST

static void *uf_join(void *data)
{
	for (int i = 0; i < 3; i++)
		ufiber_yield();
	counter = 1;
	return &counter;
}

START_TEST(test_ufiber_join)
{
	ufiber_t fid;
	void *retval;

	counter = 0;
	ck_ufiber_create(&fid, 0, uf_join, NULL);
	ck_ufiber_join(fid, &retval);
	ck_assert_int_eq(counter, 1);
	ck_assert_ptr_eq(retval, &counter);
}
END_TEST

static void *uf_self(void *data)
{
	other = ufiber_self();
	return NULL;
}

START_TEST(test_ufiber_self)
{
	ufiber_t fid;
	ufiber_t self = ufiber_self();

	ck_ufiber_create(&fid, 0, uf_self, NULL);
	ck_ufiber_join(fid, NULL);
	ck_assert(other != self);
	ck_assert(fid == other);
}
END_TEST

static void *uf_yield(void *data)
{
	counter++;
	return NULL;
}

START_TEST(test_ufiber_yield)
{
	ufiber_t fid[NR_FIBERS];

	for (int i = 0; i < NR_FIBERS; i++)
		ck_ufiber_create(&fid[i], 0, uf_yield, NULL);

	counter = 0;
	for (int i = 0; i < NR_FIBERS; i++)
		ufiber_yield();
	ck_assert_int_eq(counter, NR_FIBERS);
}
END_TEST

struct s_yield_to {
	int ord;
	ufiber_t next;
};

static void *uf_yield_to(void *data)
{
	struct s_yield_to *s = data;
	ck_assert_int_eq(s->ord, counter);
	counter++;
	ufiber_yield_to(s->next);
	return NULL;
}

START_TEST(test_ufiber_yield_to)
{
	ufiber_t fid[NR_FIBERS + 1];
	struct s_yield_to s[NR_FIBERS];

	for (int i = 0; i < NR_FIBERS; i++)
		ck_ufiber_create(&fid[i], 0, uf_yield_to, &s[i]);
	fid[NR_FIBERS] = ufiber_self();

	for (int i = 0; i < NR_FIBERS; i++) {
		s[i].ord = i;
		s[i].next = fid[i + 1];
	}

	counter = 0;
	ufiber_yield_to(fid[0]);
	ck_assert_int_eq(counter, NR_FIBERS);
}
END_TEST

static void *uf_exit(void *data)
{
	ufiber_exit(NULL);
	ck_abort_msg("running after ufiber_exit()");
	return NULL;
}

START_TEST(test_ufiber_exit)
{
	ufiber_t fid;
	ck_ufiber_create(&fid, 0, uf_exit, NULL);
	ufiber_join(fid, NULL);
}
END_TEST

static void *uf_mutex(void *data)
{
	ufiber_mutex_lock(&mutex);
	for (int i = 0; i < NR_FIBERS; i++) {
		counter = *((int*)data);
		ufiber_yield();
		ck_assert_int_eq(*((int*)data), counter);
	}
	ufiber_mutex_unlock(&mutex);
	return NULL;
}

START_TEST(test_ufiber_mutex)
{
	int uid[NR_FIBERS];
	ufiber_t fid[NR_FIBERS];

	ufiber_mutex_init(&mutex);
	for (int i = 0; i < NR_FIBERS; i++) {
		uid[i] = i;
		ck_ufiber_create(&fid[i], 0, uf_mutex, &uid[i]);
	}
	for (int i = 0; i < NR_FIBERS; i++) {
		ck_assert_int_eq(ufiber_join(fid[i], NULL), 0);
	}

	ck_assert_int_eq(ufiber_mutex_trylock(&mutex), 0);
	ck_assert_int_eq(ufiber_mutex_trylock(&mutex), EBUSY);
}
END_TEST

static void *uf_barrier(void *data)
{
	ufiber_barrier_wait(&barrier);
	(*((int*)data))++;
	return NULL;
}

START_TEST(test_ufiber_barrier)
{
	int counts[NR_FIBERS];
	ufiber_t fid[NR_FIBERS];

	ufiber_barrier_init(&barrier, NR_FIBERS+1);
	for (int i = 0; i < NR_FIBERS; i++) {
		counts[i] = 0;
		ck_ufiber_create(&fid[i], 0, uf_barrier, &counts[i]);
	}

	for (int i = 0; i < NR_FIBERS; i++) {
		ufiber_yield_to(fid[i]);
		ck_assert_int_eq(counts[i], 0);
	}
	ufiber_barrier_wait(&barrier);
	for (int i = 0; i < NR_FIBERS; i++) {
		ufiber_join(fid[i], NULL);
		ck_assert_int_eq(counts[i], 1);
	}
}
END_TEST

static void *uf_rwlock_reader(void *data)
{
	ufiber_rwlock_rdlock(&rwlock);
	ck_assert_int_eq(counter, 0);
	ufiber_barrier_wait(&barrier);
	ck_assert_int_eq(counter, 0);
	ufiber_rwlock_unlock(&rwlock);
	return NULL;
}

static void *uf_rwlock_writer(void *data)
{
	ufiber_rwlock_wrlock(&rwlock);
	counter = 1;
	ufiber_rwlock_unlock(&rwlock);
	return NULL;
}

START_TEST(test_ufiber_rwlock)
{
	ufiber_t writer;
	ufiber_t fid[NR_FIBERS];

	ufiber_barrier_init(&barrier, NR_FIBERS+1);
	ufiber_rwlock_init(&rwlock);

	for (int i = 0; i < NR_FIBERS; i++) {
		ck_ufiber_create(&fid[i], 0, uf_rwlock_reader, NULL);
		ufiber_yield_to(fid[i]);
	}
	ck_ufiber_create(&writer, 0, uf_rwlock_writer, NULL);
	ufiber_yield_to(writer);
	ufiber_barrier_wait(&barrier);
	ck_ufiber_join(writer, NULL);
	for (int i = 0; i < NR_FIBERS; i++)
		ck_ufiber_join(fid[i], NULL);
}
END_TEST

static void *uf_cond(void *data)
{
	ufiber_cond_wait(&cond, NULL);
	counter++;
	return NULL;
}

START_TEST(test_ufiber_cond)
{
	ufiber_t fid[NR_FIBERS];

	counter = 0;
	ufiber_cond_init(&cond);
	for (int i = 0; i < NR_FIBERS; i++) {
		ck_ufiber_create(&fid[i], 0, uf_cond, NULL);
		ufiber_yield_to(fid[i]);
	}

	ck_assert_int_eq(counter, 0);
	ufiber_cond_signal(&cond);
	for (int i = 0; i < NR_FIBERS; i++)
		ufiber_yield();

	ck_assert_int_eq(counter, 1);
	ufiber_cond_broadcast(&cond);
	for (int i = 0; i < NR_FIBERS; i++)
		ufiber_join(fid[i], NULL);

	ck_assert_int_eq(counter, NR_FIBERS);
}
END_TEST

START_TEST(test_deadlock)
{
	ufiber_mutex_init(&mutex);
	ufiber_mutex_lock(&mutex);
	ck_assert(ufiber_mutex_lock(&mutex) == EDEADLK);
}
END_TEST

int main(void)
{
	int failed;
	TCase *tc;
	Suite *s;
	SRunner *sr;

	ufiber_init();
	s = suite_create("ufibers");
	tc = tcase_create("core");
	tcase_add_test(tc, test_ufiber_create);
	tcase_add_test(tc, test_ufiber_join);
	tcase_add_test(tc, test_ufiber_self);
	tcase_add_test(tc, test_ufiber_yield);
	tcase_add_test(tc, test_ufiber_yield_to);
	tcase_add_test(tc, test_ufiber_exit);
	tcase_add_test(tc, test_ufiber_mutex);
	tcase_add_test(tc, test_ufiber_barrier);
	tcase_add_test(tc, test_ufiber_rwlock);
	tcase_add_test(tc, test_ufiber_cond);
	tcase_add_test(tc, test_deadlock);
	suite_add_tcase(s, tc);

	sr = srunner_create(s);
	srunner_run_all(sr, CK_NORMAL);
	failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
