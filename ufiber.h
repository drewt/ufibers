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

#ifndef _FIBER_H_
#define _FIBER_H_

#include <errno.h>
#include "queue.h"

#define UFIBER_DETACHED 1
#define UFIBER_BARRIER_SERIAL_FIBER (-1)

struct ufiber;

UFIBER_CIRCLEQ_HEAD(ufiber_waitlist, ufiber);

struct ufiber_blocklist {
	struct ufiber_waitlist blocked;
	unsigned count;
};

struct ufiber_rwlock {
	struct ufiber_waitlist rdblocked;
	struct ufiber_waitlist wrblocked;
	long reading;
};

typedef struct ufiber* ufiber_t;
typedef struct ufiber_blocklist ufiber_mutex_t;
typedef struct ufiber_blocklist ufiber_barrier_t;
typedef struct ufiber_rwlock ufiber_rwlock_t;
typedef struct ufiber_waitlist ufiber_cond_t;

int ufiber_init(void);

ufiber_t ufiber_self(void);

int ufiber_create(ufiber_t *fiber, unsigned long flags,
		void *(*start_routine)(void*), void *arg);

int ufiber_join(ufiber_t fiber, void **retval);

void ufiber_yield(void);

int ufiber_yield_to(ufiber_t fiber);

void ufiber_exit(void *retval);

void ufiber_ref(ufiber_t fiber);

void ufiber_unref(ufiber_t fiber);

static inline int ufiber_mutex_init(ufiber_mutex_t *mutex)
{
	UFIBER_CIRCLEQ_INIT(&mutex->blocked);
	mutex->count = 0;
	return 0;
}

int ufiber_mutex_destroy(ufiber_mutex_t *mutex);

int ufiber_mutex_lock(ufiber_mutex_t *mutex);

int ufiber_mutex_unlock(ufiber_mutex_t *mutex);

static inline int ufiber_mutex_trylock(ufiber_mutex_t *mutex)
{
	if (mutex->count)
		return EBUSY;
	return ufiber_mutex_lock(mutex);
}

static inline int ufiber_barrier_init(ufiber_barrier_t *barrier,
		unsigned count)
{
	UFIBER_CIRCLEQ_INIT(&barrier->blocked);
	barrier->count = count;
	return 0;
}

int ufiber_barrier_destroy(ufiber_barrier_t *barrier);

int ufiber_barrier_wait(ufiber_barrier_t *barrier);

static inline int ufiber_rwlock_init(ufiber_rwlock_t *lock)
{
	UFIBER_CIRCLEQ_INIT(&lock->rdblocked);
	UFIBER_CIRCLEQ_INIT(&lock->wrblocked);
	lock->reading = 0;
	return 0;
}

int ufiber_rwlock_destroy(ufiber_rwlock_t *lock);

int ufiber_rwlock_rdlock(ufiber_rwlock_t *lock);

int ufiber_rwlock_wrlock(ufiber_rwlock_t *lock);

static inline int ufiber_rwlock_tryrdlock(ufiber_rwlock_t *lock)
{
	if (lock->reading == -1 || !UFIBER_CIRCLEQ_EMPTY(&lock->wrblocked))
		return EBUSY;
	return ufiber_rwlock_rdlock(lock);
}

static inline int ufiber_rwlock_trywrlock(ufiber_rwlock_t *lock)
{
	if (lock->reading > 0)
		return EBUSY;
	return ufiber_rwlock_wrlock(lock);
}

int ufiber_rwlock_unlock(ufiber_rwlock_t *lock);

static inline int ufiber_cond_init(ufiber_cond_t *cond)
{
	UFIBER_CIRCLEQ_INIT(cond);
	return 0;
}

int ufiber_cond_destroy(ufiber_cond_t *cond);

int ufiber_cond_wait(ufiber_cond_t *cond, ufiber_mutex_t *mutex);

int ufiber_cond_broadcast(ufiber_cond_t *cond);

int ufiber_cond_signal(ufiber_cond_t *cond);

#endif
