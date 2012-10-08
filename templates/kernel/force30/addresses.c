/*	@(#)addresses.c	1.3	94/04/05 16:40:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Addresses for Force30
 */

#define TIA	7
#define TIB	7
#define TIC	5

#define OBMEMSIZE	0x400000
struct user_mem_chunks user_mem_chunks[] = {
	{ OBMEMBASE,			OBMEMSIZE,		0},
};

struct kernel_mem_chunks kernel_mem_chunks[] = {
/*	{ OBMEMSIZE, 	0xFA000000-OBMEMSIZE,	FL_CI },/* VME32 */
/*	{ 0xFA000000,	0x01000000,	FL_CI },	/* Message broadcast */
/*	{ 0xFB000000,	0x00FF0000,	FL_CI },	/* VME A24D32 */
/*	{ 0xFBFF0000,	0x00010000,	FL_CI },	/* VME A16D32 */
	{ 0xFC000000,	0x00FF0000,	FL_CI },	/* VME A24D16 */
	{ 0xFCFF0000,	0x00010000,	FL_CI },	/* VME A16D16 */
	{ 0xFEF00000,	0x00010000,	FL_CI },	/* Lance RAM */
	{ 0xFEF80000,	4,		FL_CI },	/* Lance */
	{ 0xFF000000,	0x00800000,	FL_WP },	/* User EPROM */
	{ 0xFF800C00,	0x20,		FL_CI },	/* PI/T1 */
	{ 0xFF800E00,	0x20,		FL_CI },	/* PI/T2 */
	{ 0xFF802000,	0x40,		FL_CI },	/* DUSCC1 */
	{ 0xFF802200,	0x40,		FL_CI },	/* DUSCC2 */
	{ 0xFF803000,	0x10,		FL_CI },	/* RTC */
/*	{ 0xFF803400,	0x10,		FL_CI },	/* MB87031 */
/*	{ 0xFF803800,	0x4,		FL_CI },	/* WD1772 */
	{ 0xFFC00000,	0x00100000,	0     },	/* Local static RAM */
	{ 0xFFD00000,	0x00100000,	FL_CI },	/* FGA-002 */
/*	{ 0xFFE00000,	0x00100000,	FL_WP },	/* System EPROM */
};

/* #define BURST_ALLOWED	/* if burst mode works */

#define BOOTPROM_ADDR	0xFF000000
