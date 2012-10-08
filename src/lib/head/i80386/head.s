/*	@(#)head.s	1.6	96/02/27 10:58:37 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifdef ACK_ASSEMBLER
! C runtime startoff for Amoeba - i80386 version
!
! The stack looks as follows when the code starts running:
!
!	( arguments, string environment and capability environment) 
!	capv
!	envp
!	argv
! sp ->	argc
!
!
.define begtext,begdata,begbss
.define ERANGE,ESET,EIDIVZ,EHEAP,EILLINS,EODDZ,ECASE
.define EXIT
.define hol0,.lino,.filn,.reghp,.limhp,.trpim,.trppc,.ignmask
.define __thread_local,_capv,_am_sched,_am_frozen
.define _environ
.sect .text
.sect .rom
.sect .data
.sect .bss

! force linking in _write from libamoeba.a: it is needed for .trp
.extern _write

! runtime startoff for i80386 machine

ERANGE          = 1
ESET            = 2
EIDIVZ          = 6
EHEAP           = 17
EILLINS         = 18
EODDZ           = 19
ECASE           = 20

.sect .text
begtext:	
	xor	bp,bp		! cleanly terminate the bp chain
	mov	ax,sp
	push	ax
	call	__stackfix
	add	sp,4
	mov	ax,8(sp)
	mov	(_environ),ax
	mov	ax,12(sp)
	mov	(_capv),ax
	call	_m_a_i_n	! main(argc, argv, envp, capv)
	add	sp, 16		! Makes stacktrace prettier
	push	ax
	call	_exit		! Does not return.

EXIT:
	call	__exit

.sect .data
begdata:
	.data4	0
hol0:	
.lino:
	.data4	0
.filn:
	.data4	0
.reghp:
	.data4	endbss
.limhp:
	.data4	endbss
.ignmask:
	.data4	0
.trpim:
	.data4	0
.trppc:
	.data4	0
__thread_local:
	.data4	-1
_capv:
	.data4	0
_environ:
	.data4 0
_am_sched:
	.data4	0
_am_frozen:
	.data4	0

	.sect .bss
begbss:	
#else

#ifdef GNU_ASSEMBLER
#define _thread_local	__thread_local
#define capv		_capv
#define am_sched	_am_sched
#define environ		_environ
#define _start		__start
#define _stackfix	__stackfix
#define main		_main
#define exit		_exit
#endif

.globl	_thread_local
.globl	capv
.globl	am_sched
.globl	am_frozen
.globl	environ

#ifndef GNU_ASSEMBLER
/* Some 386 c compilers generate references to this
** in files that use floating point:
*/
.globl	__fltused
__fltused = 1
#endif

.text

.globl	_start
_start:
	xorl	%ebp,%ebp		
	movl	%esp,%eax
	pushl	%eax
	call	_stackfix
	addl	$4, %esp
	movl	8(%esp),%eax
	movl	%eax,environ
	movl	12(%esp), %eax
	movl	%eax, capv
	call	main	
	pushl	%eax
	call	exit		

.data
_thread_local:
	.long	-1
capv:
	.long	0
environ:
	.long	0
am_sched:
	.long	0
am_frozen:
	.long	0

#endif /* ACK_ASSEMBLER */
