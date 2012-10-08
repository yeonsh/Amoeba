/*	@(#)setjmp.s	1.3	94/04/07 10:40:16 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifdef ACK_ASSEMBLER
.sect .text; .sect .rom; .sect .data; .sect .bss
.sect .bss
_gtobuf: .space	12
.extern ___setjmp
.sect .text
___setjmp:
	push ebp
	mov ebp,esp
#ifdef _POSIX_SOURCE
	mov edx,8(ebp)
	mov ecx,12(ebp)
	mov 16(edx),ecx
	cmp 12(ebp),0
	je I1_1
	mov edx,8(ebp)
	add edx,12
	push edx
	call ___newsigset
	pop ecx
I1_1:
#endif
	mov ebx,8(ebp)
	push 0(ebp)
	lea eax,8(ebp)
	push eax
	push 16(ebp)
	mov ecx,12
	call .sti
	xor eax,eax
	leave
	ret

_fill_ret_area:
	push ebp
	mov ebp,esp
	mov eax,8(ebp)
	leave
	ret

.extern _longjmp
_longjmp:
	push ebp
	mov ebp,esp
#ifdef _POSIX_SOURCE
	mov edx,8(ebp)
	cmp 16(edx),0
	je I3_2
	add edx,12
	push edx
	call ___oldsigset
	pop ecx
I3_2:
#endif
	push 8(ebp)
	push _gtobuf
	mov ecx,3
	call .blm
	mov edx,12(ebp)
	push edx
	or edx,edx
	jne I3_3
	pop edx
	inc edx
	push edx
I3_3:
	call _fill_ret_area
	pop ecx
	mov ebx,_gtobuf
	jmp .gto

#else
/*
** Setjmp -
*/
#ifdef GNU_ASSEMBLER
 .text
#else
 .text; .data; .text; .bss;  .text
#endif
.globl _setjmp
.globl setjmp
.globl _longjmp
.globl longjmp

_setjmp:
setjmp:
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%esi
	movl	8(%ebp),%esi	/* jmp_buf */
	movl	4(%ebp),%eax	/* Return address is target of jump. */
	movl	%eax,(%esi)
	movl	%esp,4(%esi)
	movl	%ecx,8(%esi)
	movl	%edx,12(%esi)
	movl	%edi,16(%esi)
	movl	(%esp),%eax
	movl	%eax,20(%esi)	/* old %esi */
	movl	4(%esp),%eax
	movl	%eax,24(%esi)	/* old %ebp */
	xorl	%eax,%eax
	popl	%esi
	popl	%ebp
	ret

_longjmp:
longjmp:
	movl	%esp,%ebp
	movl	4(%ebp),%esi
	movl	8(%ebp),%eax
	orl	%eax,%eax
	jne	__longjmp_out
	movl	$1,%eax
__longjmp_out:
	movl	(%esi),%ebx		/* Target of jump. */
	movl	4(%esi),%esp	/* Restore %esp now. */
	pushl	%eax		/* save %eax */
	movl	8(%esi),%ecx
	movl	12(%esi),%edx
	movl	16(%esi),%edi
	movl	20(%esi),%eax	/* save old %esi until can toss  */
	pushl	%eax		/*   one are using now */
	movl	24(%esi),%ebp	/* old %ebp */
	popl	%esi
	popl	%eax
	addl	$12,%esp		/* Toss old return address, %ebp and %esi. */
	pushl	%ebx
	ret
#endif /* ACK_ASSEMBLER */
