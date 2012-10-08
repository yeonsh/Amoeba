/*	@(#)mc68851.h	1.2	94/04/06 16:47:40 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MC68851_H__
#define __MC68851_H__

/*
 * Structure defines for mc68851 memory management unit
 */

/*
 * root pointer register (CRP, SRP, DRP)
 * This is actually a quadword but C is not so good at those
 */

struct pm_rpr {
	uint16	rpr_limit;	/* limit plus L/U bit */
	uint16	rpr_flags;	/* various */
	long    rpr_taddr;	/* table address */
};

/*
 * PCSR register, unsigned short
 */

#define PCSR_TA		0x7	/* mask to get thread alias */
#define PCSR_LW		0x4000	/* Address Translation Cache almost full */
#define PCSR_F		0x8000	/* ATC entries for TA flushed */

/*
 * TC register, unsigned long
 */

#define TC_E		0x80000000	/* translation Enable */
#define TC_SRE		0x02000000	/* SRP in use */
#define TC_FCL		0x01000000	/* Function Code Lookup in use */
#define TC_PS		0x00F00000	/* mask for Page Size */
#define TC_PS_SHIFT	20			/* shift */
#define TC_PS_256		0x00800000
#define TC_PS_512		0x00900000
#define TC_PS_1K		0x00A00000
#define TC_PS_2K		0x00B00000
#define TC_PS_4K		0x00C00000
#define TC_PS_8K		0x00D00000
#define TC_PS_16K		0x00E00000
#define TC_PS_32K		0x00F00000
#define TC_IS		0x000F0000	/* mask for Initial Shift */
#define TC_IS_SHIFT	16			/* shift */
#define TC_TIA		0x0000F000	/* Number of bits in first level */
#define TC_TIA_SHIFT	12
#define TC_TIB		0x00000F00	/* Number of bits in second level */
#define TC_TIB_SHIFT	8
#define TC_TIC		0x000000F0	/* Number of bits in third level */
#define TC_TIC_SHIFT	4
#define TC_TID		0x0000000F	/* Number of bits in fourth level */
#define TC_TID_SHIFT	0

/*
 * CAL, VAL, SCC and AC unused at the moment
 */

/*
 * PSR register, unsigned short, set by PTEST instruction
 */

#define PSR_B		0x8000		/* Bus error */
#define PSR_L		0x4000		/* Limit violation */
#define PSR_S		0x2000		/* Supervisor violation */
#define PSR_A		0x1000		/* Access level violation */
#define PSR_W		0x0800		/* Write protected */
#define PSR_I		0x0400		/* Invalid */
#define PSR_M		0x0200		/* Modified */
#define PSR_G		0x0100		/* Gate */
#define PSR_C		0x0080		/* globally sharable */
#define PSR_N		0x0007		/* mask for level Number */

/*
 * BAD? and BAC? unused
 */

/*
 * Descriptor formats
 */

struct pm_sftd {
	long	sftd_adrflags;
};

struct pm_lftd {
	uint16	lftd_limit;
	uint16	lftd_flags;
	long	lftd_taddr;
};

struct pm_lfpd {
	uint16	lfpd_unused;
	uint16	lfpd_flags;
	long	lfpd_paddr;
};

#define LIM_UPPER	0x0000
#define LIM_LOWER	0x8000
#define LIM_UNLIM	0x7FFF

#define FL_DT_MASK	0x0003
#define FL_DT_INVALID	0x0000
#define FL_DT_PAGE	0x0001
#define FL_DT_V4BYTE	0x0002
#define FL_DT_V8BYTE	0x0003

#define FL_WP		0x0004
#define FL_U		0x0008
#define FL_M		0x0010
#define FL_L		0x0020
#define FL_CI		0x0040
#define FL_G		0x0080
#define FL_S		0x0100
#define FL_SG		0x0200
#define FL_WALMASK	0x1C00
#define FL_RALMASK	0xE000

long PgetTc();

#endif /* __MC68851_H__ */
