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

void *shared = NULL;

static void *producer(void *data)
{
	shared = producer;
	printf("Producer: 0x%p -> shared\n", shared);
	return NULL;
}

static void *consumer(void *data)
{
	fiber_t fid;

	fiber_create(&fid, 0, producer, NULL);
	fiber_join(fid, NULL);
	printf("Consumer: 0x%p <- shared\n", shared);
	return shared;
}

int main(void)
{
	fiber_t fid;
	void *val;

	fiber_init();
	fiber_create(&fid, 0, consumer, NULL);
	fiber_join(fid, &val);
	printf("Root:     0x%p <- child\n", val);
}
