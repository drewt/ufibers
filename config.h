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

#ifndef _CONFIG_H_
#define _CONFIG_H_

/* compiler */
#if __GNUC__
# if __i386__
#  define ARCH_IA32 1
# elif __amd64__
#  define ARCH_AMD64 1
# else
#  error unsupported architecture
# endif /* __i386__ */
#else
# error unsupported compiler
#endif /* GCC */

#define STACK_SIZE (8*1024*1024)

#endif
