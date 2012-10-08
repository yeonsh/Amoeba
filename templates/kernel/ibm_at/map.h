/*	@(#)map.h	1.8	96/02/16 15:45:21 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * map.h
 *
 * Memory mapping constants for ISA-bus (IBM/AT) type machine.
 */

/*
 * i80386 Amoeba memory map:
 *
 *	+----------------+ 0x000000
 *	|     page 0     |		(mapped out)
 *	+----------------+ 0x001000
 *	|    boot GDT    |		(only used during reboot)
 *	+----------------+ 0x001800
 *	|   boot info    |		(boot info/parameters)
 *	+----------------+ 0x002000
 *	|                |
 *	|................|
 *	|  Amoeba kernel |
 *	|................|
 *	|                |
 *	+----------------+ 0x100000
 *	|  print buffer  |
 *	+----------------+ 0x108000
 *	|                |
 *	|   user space   |
 *	|                |
 *	|................|
 */

/* 
 * The kernel is loaded at 0x2000 (8K) and is linked to start at
 * that address so kernel virtual = physical.
 */

/*
 * The print buffer is located just after the one mega-byte limit. This
 * prevents overwritting the buffer during system boot.
 */
#define PB_START	((struct printbuf *) 0x100000)
#define PB_SIZE		30000

/*
 * LOW_MEM_START is where low memory starts (physically)
 */
#define LOW_MEM_START	0x0

/*
 * HI_MEM_START is where high memory starts (physically)
 */
#define HI_MEM_START	0x100000

/*
 * The following is the amount of memory to be left over for user
 * processes when a bullet server is to run on the machine.
 */
#define	USERSIZE	0x100000

/*
 * Number of segments for bootstrapping
 */
#define N_SEGS		10

/*
 * Amount of kernel stack for a user program.
 * The default suffices for Amoeba 4.0, but FLIP is very greedy.
 */
#define	KTHREAD_STKSIZ	8192

#define PTOV(x)		((vir_bytes) (x))
#define VTOP(x)		((phys_bytes) (x))
