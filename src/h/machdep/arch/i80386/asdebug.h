/*	@(#)asdebug.h	1.3	94/04/06 15:56:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __ASDEBUG_H__
#define __ASDEBUG_H__

/*
 * DEBUG macros.  The macros can only be used on ISA/MCA based
 * machines, and some of them may only be user BEFORE the jump
 * to protected mode since they use BIOS character I/O.
 */
#if defined(ISA) || defined(MCA)

/*
 * BIOS_CONS(ascii character) to print a character on the console
 */
#define BIOS_CONS(c) \
	PUSH_L	(EAX)		;\
	PUSH_L	(EBX)		;\
	MOV_B 	(c,AL)		;\
	MOV_B 	(CONST(14),AH)	;\
	MOV_B 	(CONST(1),BL)	;\
	XOR_B 	(BH,BH)		;\
	INT 	(CONST(0x10))	;\
	POP_L	(EBX)		;\
	POP_L	(EAX)

/*
 * Print CR/LF combination
 */
#define BIOS_PR_CR \
	BIOS_CONS(13)		;\
	BIOS_CONS(10)

/*
 * Print 16-bit number in hexadecimal.
 * Number is in cx register.
 */
#define BIOS_PR_NUM(AA,BB,CC,DD) \
	PUSH_L	(EBX)			;\
	PUSH_L	(EAX)			;\
	MOV_B	(CH,AL)			;\
	SHR_L	(CONST(4),EAX)		;\
	AND_L	(CONST(0xF),EAX)	;\
	ADD_L	(CONST(0x30),EAX)	;\
	CMP_L	(CONST(0x3A),EAX)	;\
	JNGE	(AA)			;\
	ADD_L	(CONST(0x7),EAX)	;\
AA:	MOV_B	(CONST(14),AH)		;\
	MOV_B	(CONST(1),BH)		;\
	XOR_B	(BL,BL)			;\
	INT	(CONST(0x10))		;\
	MOV_B	(CH,AL)			;\
	AND_L	(CONST(0xF),EAX)	;\
	ADD_L	(CONST(0x30),EAX)	;\
	CMP_L	(CONST(0x3A),EAX)	;\
	JNGE	(BB)			;\
	ADD_L	(CONST(0x7),EAX)	;\
BB:	MOV_B	(CONST(14),AH)		;\
	MOV_B	(CONST(1),BH)		;\
	XOR_B	(BL,BL)			;\
	INT	(CONST(0x10))		;\
	MOV_B	(CL,AL)			;\
	SHR_L	(CONST(4),EAX)		;\
	AND_L	(CONST(0xF),EAX)	;\
	ADD_L	(CONST(0x30),EAX)	;\
	CMP_L	(CONST(0x3A),EAX)	;\
	JNGE	(CC)			;\
	ADD_L	(CONST(0x7),EAX)	;\
CC:	MOV_B	(CONST(14),AH)		;\
	MOV_B	(CONST(1),BH)		;\
	XOR_B	(BL,BL)			;\
	INT	(CONST(0x10))		;\
	MOV_B	(CL,AL)			;\
	AND_L	(CONST(0xF),EAX)	;\
	ADD_L	(CONST(0x30),EAX)	;\
	CMP_L	(CONST(0x3A),EAX)	;\
	JNGE	(DD)			;\
	ADD_L	(CONST(0x7),EAX)	;\
DD:	MOV_B	(CONST(14),AH)		;\
	MOV_B	(CONST(1),BH)		;\
	XOR_B	(BL,BL)			;\
	INT	(CONST(0x10))		;\
	POP_L	(EAX)			;\
	POP_L	(EBX)

/*
 * ASY_CONS(ascii character) to print a character on the asy port
 */
#define ASY_CONS(c,AA) \
	PUSH_L	(EDX)			;\
	PUSH_L	(EAX)			;\
	MOV_L	(CONST(0x3F8+5),EDX)	;\
AA:	IN_B				;\
	TEST_B	(CONST(0x20),AL)	;\
	JZ	(AA)			;\
	MOV_L	(CONST(0x3F8+0),EDX)	;\
	MOV_B	(c,AL)			;\
	OUT_B				;\
	POP_L	(EAX)			;\
	POP_L	(EDX)

/*
 * Print CR/LF combination
 */
#define ASY_PR_CR(AA,BB) \
	ASY_CONS(13, AA)	;\
	ASY_CONS(10, BB)

/*
 * Print 16-bit number in hexadecimal.
 * Number is in cx register.
 */
#define ASY_PR_NUM(AA,BB,CC,DD) \
	PUSH_L	(EBX)			;\
	PUSH_L	(EAX)			;\
	MOV_B	(CH,BL)			;\
	SHR_L	(CONST(4),EBX)		;\
	AND_L	(CONST(0xF),EBX)	;\
	ADD_L	(CONST(0x30),EBX)	;\
	CMP_L	(CONST(0x3A),EBX)	;\
	MOV_L	(CONST(0x3F8+5),EDX)	;\
	JNGE	(AA)			;\
	ADD_L	(CONST(0x7),EBX)	;\
AA:	IN_B				;\
	TEST_B	(CONST(0x20),AL)	;\
	JZ	(AA)			;\
	MOV_L	(CONST(0x3F8+0),EDX)	;\
	MOV_B	(BL,AL)			;\
	OUT_B				;\
	MOV_B	(CH,BL)			;\
	AND_L	(CONST(0xF),EBX)	;\
	ADD_L	(CONST(0x30),EBX)	;\
	CMP_L	(CONST(0x3A),EBX)	;\
	MOV_L	(CONST(0x3F8+5),EDX)	;\
	JNGE	(BB)			;\
	ADD_L	(CONST(0x7),EBX)	;\
BB:	IN_B				;\
	TEST_B	(CONST(0x20),AL)	;\
	JZ	(BB)			;\
	MOV_L	(CONST(0x3F8+0),EDX)	;\
	MOV_B	(BL,AL)			;\
	OUT_B				;\
	MOV_B	(CL,BL)			;\
	SHR_L	(CONST(4),EBX)		;\
	AND_L	(CONST(0xF),EBX)	;\
	ADD_L	(CONST(0x30),EBX)	;\
	CMP_L	(CONST(0x3A),EBX)	;\
	MOV_L	(CONST(0x3F8+5),EDX)	;\
	JNGE	(CC)			;\
	ADD_L	(CONST(0x7),EBX)	;\
CC:	IN_B				;\
	TEST_B	(CONST(0x20),AL)	;\
	JZ	(CC)			;\
	MOV_L	(CONST(0x3F8+0),EDX)	;\
	MOV_B	(BL,AL)			;\
	OUT_B				;\
	MOV_B	(CL,BL)			;\
	AND_L	(CONST(0xF),EBX)	;\
	ADD_L	(CONST(0x30),EBX)	;\
	CMP_L	(CONST(0x3A),EBX)	;\
	MOV_L	(CONST(0x3F8+5),EDX)	;\
	JNGE	(DD)			;\
	ADD_L	(CONST(0x7),EBX)	;\
DD:	IN_B				;\
	TEST_B	(CONST(0x20),AL)	;\
	JZ	(DD)			;\
	MOV_L	(CONST(0x3F8+0),EDX)	;\
	MOV_B	(BL,AL)			;\
	OUT_B				;\
	POP_L	(EAX)			;\
	POP_L	(EBX)

/*
 * Turn bell on/off
 */
#define BELL_ON \
	PUSH_L	(EAX)			;\
	PUSH_L	(EDX)			;\
	MOV_L	(CONST(0x61),EDX)	;\
	IN_B				;\
	OR_B	(CONST(0x03),AL)	;\
	OUT1_B	(DX)			;\
	POP_L	(EDX)			;\
	POP_L	(EAX)

#define BELL_OFF \
	PUSH_L	(EAX)			;\
	PUSH_L	(EDX)			;\
	MOV_L	(CONST(0x61),EDX)	;\
	IN_B				;\
	AND_B	(CONST(ENOT(0x03)),AL)	;\
	OUT1_B	(DX)			;\
	POP_L	(EDX)			;\
	POP_L	(EAX)

#endif /* defined(ISA) || defined(MCA) */
#endif /* __ASDEBUG_H__ */
