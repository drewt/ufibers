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

#include "config.h"

.global __context_switch
.global trampoline

#if ARCH_AMD64
__context_switch:
	pushfq
	pushq %rbx
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	pushq %rbp
	movq  %rsp, (%rdi)
	movq  (%rsi), %rsp
	popq  %rbp
	popq  %r15
	popq  %r14
	popq  %r13
	popq  %r12
	popq  %rbx
	popfq
	ret

# start_routine in %r14, arg in %r15
trampoline:
	movq %r15, %rdi
	call *%r14
	movq %rax, %rdi
	call ufiber_exit

#elif ARCH_IA32
__context_switch:
	pushfl
	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %esi
	pushl %edi
	pushl %ebp
	movl  %esp, -4(%esp) # XXX: this is wrong!
	movl  -8(%esp), %esp # this too!
	popl  %ebp
	popl  %edi
	popl  %esi
	popl  %edx
	popl  %ecx
	popl  %ebx
	popl  %eax
	popfl
	ret

# start_routine in %eax, arg in %ebx
trampoline:
	pushl %ebx
	call  *%eax
	pushl %eax
	call ufiber_exit

#endif
