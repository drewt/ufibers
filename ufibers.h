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

typedef unsigned long fiber_t;

int fiber_init(void);

fiber_t fiber_self(void);

int fiber_create(fiber_t *fiber, unsigned long flags,
		void *(*start_routine)(void*), void *arg);

int fiber_join(fiber_t fiber, void **retval);

void fiber_yeild(void);

int fiber_yeild_to(fiber_t fiber);

void fiber_exit(void *retval);

#endif
