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

#include "list.h"

#define FF_NOREF 1

#define UFIBER_BARRIER_SERIAL_FIBER (-1)

struct ufiber_blocklist {
	struct list_head blocked;
	unsigned count;
};

typedef struct fiber* ufiber_t;
typedef struct ufiber_blocklist ufiber_mutex_t;
typedef struct ufiber_blocklist ufiber_barrier_t;

int ufiber_init(void);

ufiber_t ufiber_self(void);

int ufiber_create(ufiber_t *fiber, unsigned long flags,
		void *(*start_routine)(void*), void *arg);

int ufiber_join(ufiber_t fiber, void **retval);

void ufiber_yeild(void);

int ufiber_yeild_to(ufiber_t fiber);

void ufiber_exit(void *retval);

void ufiber_ref(ufiber_t fiber);

void ufiber_unref(ufiber_t fiber);

static inline int ufiber_mutex_init(ufiber_mutex_t *mutex)
{
	INIT_LIST_HEAD(&mutex->blocked);
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
	INIT_LIST_HEAD(&barrier->blocked);
	barrier->count = count;
	return 0;
}

int ufiber_barrier_destroy(ufiber_barrier_t *barrier);

int ufiber_barrier_wait(ufiber_barrier_t *barrier);

#endif
