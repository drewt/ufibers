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

#include "config.h"

.global _ufiber_create
.global _ufiber_switch
.global _ufiber_trampoline

#if ARCH_AMD64

# _ufiber_create(cx_addr, start_routine, arg, trampoline, exit)
_ufiber_create:
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

# _ufiber_switch(unsigned long *save_sp, unsigned long *rest_sp)
_ufiber_switch:
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
_ufiber_trampoline:
	movq %r15, %rdi
	call *%r14
	movq %rax, %rdi
	call *%r13

#elif ARCH_IA32

# _ufiber_create(cx_addr, start_routine, arg, trampoline, exit)
_ufiber_create:
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

# _ufiber_switch(unsigned long *save_sp, unsigned long *rest_sp)
_ufiber_switch:
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
_ufiber_trampoline:
	pushl %edi
	call  *%esi
	pushl %eax
	call  *%ebx

#elif ARCH_ARM

@ _ufiber_create(cx_addr, start_routine, arg, trampoline, exit)
_ufiber_create:
	ldr  r12, [sp]     @ pull arg 5 (exit) off the stack
	push {r4}
	mov  r4, sp        @ save stack pointer in r4
	mov  sp, r0        @ move the stack pointer to cx_addr
	sub  sp, sp, #4    @ fix off-by-one in cx_addr
	@ TODO: use block transfer somehow
	push {r1}          @ r4 := start_routine
	push {r2}          @ r5 := arg
	push {r12}         @ r6 := exit
	push {r7}          @ r7
	push {r8}          @ r8
	push {r9}          @ r9
	push {r10}         @ r10
	push {r11}         @ r11
	mrs  r0, cpsr
	push {r0}          @ cpsr
	push {r3}          @ lr := trampoline
	mov  r0, sp
	mov  sp, r4        @ restore stack pointer
	pop  {r4}
	bx   lr

@ _ufiber_switch(unsigned long *save_sp, unsigned long *rest_sp)
_ufiber_switch:
	mrs  r12, cpsr
	@ TODO: block transfer
	push {r4}
	push {r5}
	push {r6}
	push {r7}
	push {r8}
	push {r9}
	push {r10}
	push {r11}
	push {r12}
	push {lr}
	str  sp, [r0]      @ save stack pointer in save_sp
	ldr  sp, [r1]      @ restore stack pointer from rest_sp
	@ TODO: block transfer
	pop  {lr}
	pop  {r12}
	pop  {r11}
	pop  {r10}
	pop  {r9}
	pop  {r8}
	pop  {r7}
	pop  {r6}
	pop  {r5}
	pop  {r4}
	msr  cpsr, r12
	bx   lr

@ ufiber_exit in r6, start_routine in r4, arg in r5
_ufiber_trampoline:
	mov r0, r5
	blx r4
	bx  r6

#endif
