/*	@(#)bblock.h	1.2	94/04/06 16:43:34 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __BBLOCK_H__
#define __BBLOCK_H__

/*
** bblock.h, layout of block 0 structure.
**
*/

/*
** boot block structure, taken from the KA650 CPU module technical manual
** and /usr/src/sys/sas/bootblk.c.
**
*/
typedef struct {
	/* part one,  first 8 bytes of boot block */
	uint16	b1_any;		/* any value */
	uint8	b1_n;		/* n points to table at offset 2*n */
	uint8	b1_one;		/* must be one */
	uint16	b1_hbn;		/* high block number of image start */
	uint16	b1_lbn;		/* low block number of image start */
} b1_block;

typedef struct {
	/* part two, configuration table at offset 2*n in bootblock */
	uint16  b2_instr;	/* Instruction set type */
	uint8	b2_k;		/* File system type (for amoeba 0) */
	uint8	b2_chk;		/* Check byte (~(b2_instr + b2_k)) */
	uint32	b2_any;		/* Any value, most likely 0 */
	uint32	b2_nblock;	/* Number of blocks in image */
	uint32	b2_loadaddr;	/* In core load address */
	uint32  b2_start;	/* Image entry point */
	uint32	b2_sum;		/* Sum of previous three words */
} b2_block;

#endif /* __BBLOCK_H__ */
