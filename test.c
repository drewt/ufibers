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
	ufiber_t fid;

	ufiber_create(&fid, 0, producer, NULL);
	ufiber_join(fid, NULL);
	ufiber_unref(fid);
	printf("Consumer: 0x%p <- shared\n", shared);
	return shared;
}

int main(void)
{
	ufiber_t fid;
	void *val;

	ufiber_init();
	ufiber_create(&fid, 0, consumer, NULL);
	ufiber_join(fid, &val);
	ufiber_unref(fid);
	printf("Root:     0x%p <- child\n", val);

	ufiber_create(NULL, 0, consumer, NULL);
	ufiber_create(NULL, 0, producer, NULL);
	ufiber_create(NULL, 0, producer, NULL);
	ufiber_create(NULL, 0, producer, NULL);
	ufiber_create(NULL, 0, producer, NULL);

	ufiber_yeild();
	ufiber_yeild();
	ufiber_yeild();
	ufiber_yeild();
	ufiber_yeild();
}
