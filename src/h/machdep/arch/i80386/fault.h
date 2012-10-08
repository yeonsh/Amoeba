/*	@(#)fault.h	1.6	94/08/19 15:07:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * fault.h
 */
#ifndef __FAULT_H__
#define __FAULT_H__

struct fault {
	unsigned long	if_memmap;	/* cr3 */
	unsigned long	if_gs;		
	unsigned long	if_fs;		
	unsigned long	if_es;		
	unsigned long	if_ds;		
	unsigned long	if_regs[8];	/* edi, esi, ebp, esp, ebx, edx, ecx, eax */
#define if_edi	if_regs[0]
#define if_esi	if_regs[1]
#define if_ebp	if_regs[2]
/* if_esp is further down, this is used for fault address */
#define if_faddr if_regs[3]
#define if_ebx	if_regs[4]
#define if_edx	if_regs[5]
#define if_ecx	if_regs[6]
#define if_eax	if_regs[7]
	unsigned long	if_trap;	/* Trap type */
	unsigned long	if_errcode;	/* optional additional information */
	unsigned long	if_eip;		/* instruction pointer */
#define	pc	if_eip
	unsigned long	if_cs;		/* actually two byte word */
	unsigned long	if_eflag;	/* process status longword */
	unsigned long	if_esp;		/* sp */
#define	sp	if_esp
	unsigned long	if_ss;		/* ss */
};

/*
 * Location of the users' stored registers relative to ax
 */
#define SS	19
#define UESP	18
#define EFL	17
#define CS	16
#define EIP	15
#define ERRR	14
#define TRAPNO	13
#define EAX	12
#define ECX	11
#define EDX	10
#define EBX	9
#define ESP	8
#define EBP	7
#define ESI	6
#define EDI	5
#define DS	4
#define ES	3
#define FS	2
#define GS	1
#if 0  /* CR3 is used in our non-standard posix ioctl.h! */
#define CR3	0
#endif

typedef struct fault thread_ustate;
#define USTATE_SIZE_MIN (sizeof(struct fault))
#define USTATE_SIZE_MAX USTATE_SIZE_MIN
#define USTATE_SIZE(us) USTATE_SIZE_MIN

#define ARCHITECTURE "i80386"

#endif /* __FAULT_H__ */
