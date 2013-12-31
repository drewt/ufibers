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

.global __ufiber_create
.global __ufiber_switch
.global __ufiber_trampoline

#if ARCH_AMD64

# __ufiber_create(cx_addr, start_routine, arg)
__ufiber_create:
	movq  %rsp, %rax
	movq  %rdi, %rsp
	pushq %rcx
	pushfq
	pushq $0
	pushq $0
	pushq %r8
	pushq %rsi
	pushq %rdx
	movq  %rax, %rsp
	ret

__ufiber_switch:
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

# ufiber_exit in %r13, start_routine in %r14, arg in %r15
__ufiber_trampoline:
	movq %r15, %rdi
	call *%r14
	movq %rax, %rdi
	call *%r13

#elif ARCH_IA32

__ufiber_create:
	pushl %ebx
	pushl %edi
	movl    %esp,  %eax
	movl 16(%esp), %ecx
	movl 20(%esp), %edx
	movl 24(%esp), %ebx
	movl 28(%esp), %edi
	movl 12(%esp), %esp
	pushl %ebx
	pushfl
	pushl %edi
	pushl %ecx
	pushl %edx
	movl  %eax, %esp
	popl  %edi
	popl  %ebx
	ret

__ufiber_switch:
	movl  4(%esp), %eax
	movl  8(%esp), %ecx
	pushfl
	pushl %ebx
	pushl %esi
	pushl %edi
	pushl %ebp
	movl  %esp, (%eax)
	movl  (%ecx), %esp
	popl  %ebp
	popl  %edi
	popl  %esi
	popl  %ebx
	popfl
	ret

# ufiber_exit in %ebx, start_routine in %esi, arg in %edi
__ufiber_trampoline:
	pushl %edi
	call  *%esi
	pushl %eax
	call  *%ebx

#endif
