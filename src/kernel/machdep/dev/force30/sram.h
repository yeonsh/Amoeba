/*	@(#)sram.h	1.4	94/04/06 09:07:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Force static battery backuped ram
 */

#define SRAMMEM	6*4096

struct f30sram {
    union {
	char f3s4k_bytes[4096];
	short f3s4k_short[2048];
	long f3s4k_long[1024];
	struct {
	    char	f3s_ident[4];			/* 000 */
	    long	f3s_chksum;			/* 004 */
	    char	f3s_unused1[0x800-8];		/* 008 */
	    char	f3s_reset_reason;		/* 800 */
	    char	f3s_slot_number;		/* 801 */
	    char	f3s_reserved1[14];		/* 802 */
	    short	f3s_cpu_type;			/* 810 */
	    short	f3s_mem_size;			/* 812 */
	    short	f3s_clock_speed;		/* 814 */
	    char	f3s_reserved2[10];		/* 816 */
	    char	f3s_rotary_switch;		/* 820 */
	    char	f3s_reserved3[4096-0x821];	/* 821 */
	} f3s4k_struct;
    } f30sram_boot_setup;
    char f30sram_roundof[4096];
    char f30sram_mem[SRAMMEM];
};

#define F30SRAM	((struct f30sram *) 0xFFC00000)
