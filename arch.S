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

.global _ufiber_create
.global _ufiber_switch
.global _ufiber_trampoline

#if __amd64__

# AMD64 CPU state is saved as follows:
#
#     struct amd64_context {
#         word_t return_addr;
#         word_t eflags;
#         word_t rbx;
#         word_t r12;
#         word_t r13;
#         word_t r14;
#         word_t r15;
#         word_t rbp;
#     };

# _ufiber_create(stack, stack_size, start_routine, arg, trampoline, exit)
_ufiber_create:
	movq  %rsp, %rax
	movq  %rdi, %rsp  # rsp := stack
	addq  %rsi, %rsp  # rsp += stack_size
	pushq %r8         # return address := trampoline
	pushfq
	pushq $0
	pushq $0
	pushq %r9         # cx.r13 := ufiber_exit
	pushq %rdx        # cx.r14 := start_routine
	pushq %rcx        # cx.r15 := arg
	pushq $0
	xchg  %rax, %rsp
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

#elif __i386__

# IA32 CPU state is saved as follows:
#
#     struct ia32_context {
#         word_t return_addr;
#         word_t eflags;
#         word_t ebx;
#         word_t esi;
#         word_t edi;
#         word_t edi;
#         word_t ebp;
#     };

# _ufiber_create(stack, stack_size, start_routine, arg, trampoline, exit)
_ufiber_create:
	pushl %ebx
	pushl %edi
	movl    %esp,  %eax
	movl 20(%esp), %ecx  # ecx := start_routine
	movl 24(%esp), %edx  # edx := arg
	movl 28(%esp), %ebx  # ebx := trampoline
	movl 32(%esp), %edi  # edi := exit
	movl 12(%esp), %esp  # esp := stack
	addl 16(%eax), %esp  # esp += stack_size
	pushl %ebx           # cx.return address := trampoline
	pushfl
	pushl %edi           # cx.ebx := ufiber_exit
	pushl %ecx           # cx.esi := start_routine
	pushl %edx           # cx.edi := arg
	pushl $0
	xchg  %eax, %esp
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

#elif __arm__

@ ARM CPU state is saved as follows:
@
@     struct arm_context {
@         word_t lr;
@         word_t cpsr;
@         word_t r11;
@         word_t r10;
@         word_t r9;
@         word_t r8;
@         word_t r7;
@         word_t r6;
@         word_t r5;
@         word_t r4;
@         word_t fpscr;
@         word_t s16;
@         ...
@         word_t s31;
@     };

@ _ufiber_create(stack, stack_size, start_routine, arg, trampoline, exit)
_ufiber_create:
	push {r4, r5}
	ldr  r4,  [sp, #8]   @ r4  := trampoline
	ldr  r5,  [sp, #12]  @ r5  := exit
	mov  r12, sp         @ r12 := sp
	mov  sp,  r0         @ sp  := stack
	add  sp,  sp, r1     @ sp  += stack_size

	@ push initial context onto stack
	push {r4}            @ cx.lr := trampoline
	mrs  r0, cpsr
	push {r0}            @ save cpsr
	push {r7-r11}        @ (these don't actually matter)
	push {r2-r3, r5}     @ cx.{r4, r5, r6} := {start_routine, arg, exit}
#if __ARM_PCS_VFP && !ASSUME_NO_FPU
	vmrs  r0, fpscr
	push  {r0}           @ save FPU status
	vpush {s16-s31}      @ (don't matter)
#endif

	mov  r0, sp
	mov  sp, r12         @ restore stack pointer
	pop  {r4, r5}
	bx   lr

@ _ufiber_switch(unsigned long *save_sp, unsigned long *rest_sp)
_ufiber_switch:
	mrs   r12, cpsr
	push  {r4-r12, lr}
#if __ARM_PCS_VFP && !ASSUME_NO_FPU
	vmrs  r12, fpscr
	push  {r0}
	vpush {s16-s31}
#endif
	str   sp, [r0]      @ save stack pointer in save_sp
	ldr   sp, [r1]      @ restore stack pointer from rest_sp
#if __ARM_PCS_VFP && !ASSUME_NO_FPU
	pop   {r12}
	vmsr  fpscr, r12
	vpop  {s16-s31}
#endif
	pop   {r4-r12, lr}
	msr   cpsr, r12
	bx    lr

@ ufiber_exit in r6, start_routine in r4, arg in r5
_ufiber_trampoline:
	mov r0, r5
	blx r4
	bx  r6

#endif
