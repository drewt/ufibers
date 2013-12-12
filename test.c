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

static long shared[BUF_LEN];
static size_t wpos = 0;
static size_t rpos = 0;

static void *producer(void *data)
{
	if ((long)data % 2 == 0)
		ufiber_yeild();

	while (next(wpos) == rpos)
		ufiber_yeild();

	shared[wpos] = (long) data;
	wpos = next(wpos);

	return NULL;
}

static void *consumer(void *data)
{
	while (rpos == wpos)
		ufiber_yeild();

	printf("read %ld\n", shared[rpos]);
	rpos = next(rpos);

	return NULL;
}

int main(void)
{
	ufiber_t fids[NR_FIBERS * 2];
	void *rv;

	ufiber_init();
	for (unsigned long i = 0; i < NR_FIBERS; i++)
		ufiber_create(&fids[i], 0, producer, (void*) i);
	for (int i = 0; i < NR_FIBERS; i++)
		ufiber_create(&fids[i + NR_FIBERS], 0, consumer, NULL);

	for (int i = 0; i < NR_FIBERS * 2; i++) {
		ufiber_join(fids[i], &rv);
		ufiber_unref(fids[i]);
	}

	return 0;
}
