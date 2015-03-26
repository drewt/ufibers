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

#ifndef _UFIBER_H_
#define _UFIBER_H_

#define UFIBER_DETACHED 1
#define UFIBER_BARRIER_SERIAL_FIBER (-1)

struct ufiber;

struct ufiber_waitlist {
	struct ufiber *cqh_first;
	struct ufiber *cqh_last;
};

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

int ufiber_mutex_init(ufiber_mutex_t *mutex);
int ufiber_mutex_destroy(ufiber_mutex_t *mutex);
int ufiber_mutex_lock(ufiber_mutex_t *mutex);
int ufiber_mutex_unlock(ufiber_mutex_t *mutex);
int ufiber_mutex_trylock(ufiber_mutex_t *mutex);

int ufiber_barrier_init(ufiber_barrier_t *barrier, unsigned count);
int ufiber_barrier_destroy(ufiber_barrier_t *barrier);
int ufiber_barrier_wait(ufiber_barrier_t *barrier);

int ufiber_rwlock_init(ufiber_rwlock_t *lock);
int ufiber_rwlock_destroy(ufiber_rwlock_t *lock);
int ufiber_rwlock_rdlock(ufiber_rwlock_t *lock);
int ufiber_rwlock_wrlock(ufiber_rwlock_t *lock);
int ufiber_rwlock_tryrdlock(ufiber_rwlock_t *lock);
int ufiber_rwlock_trywrlock(ufiber_rwlock_t *lock);
int ufiber_rwlock_unlock(ufiber_rwlock_t *lock);

int ufiber_cond_init(ufiber_cond_t *cond);
int ufiber_cond_destroy(ufiber_cond_t *cond);
int ufiber_cond_wait(ufiber_cond_t *cond, ufiber_mutex_t *mutex);
int ufiber_cond_broadcast(ufiber_cond_t *cond);
int ufiber_cond_signal(ufiber_cond_t *cond);

#endif
