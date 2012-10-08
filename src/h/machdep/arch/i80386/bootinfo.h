/*	@(#)bootinfo.h	1.6	94/04/06 15:58:17 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __BOOTINFO_H__
#define __BOOTINFO_H__

/*
 * bootinfo.h
 *
 * The following structure is used to pass bootstrap information
 * from the bootstrap loader to the Amoeba kernel. It is especially
 * useful for passing on BIOS data, and security related secrets.
 */

#define	BI_MAGIC	0x32314204L	/* almost an unique magic */
#define	BI_ADDR		0x1800L		/* location in memory */
#define	BI_HDPARAM0	0x41		/* parameters of first disk */
#define	BI_HDPARAM1	0x46		/* parameters of second disk */

struct hdbiosinfo {
    uint16	hdi_ncyl;		/* # of cylinders */
    uint8	hdi_nheads;		/* # of heads */
    uint16	hdi_precomp;		/* write precompensation register */
    uint8	hdi_ctrl;		/* control byte */
    uint8	hdi_nsectrk;		/* # of sectors / track */
};

/*
 * The kernel boot code (either floppy or hard disk) contains some
 * security related default values. In the future this information
 * may also be stored in system ROM, or non-volatile memory.
 */
struct bootdefaults {
    capability		bd_loginsvr;	/* login server capability */
    char		bd_user[8];	/* user name */
    char		bd_passwd[8];	/* password */
    char		bd_offset[6];	/* magic for getport computation */
};

/*
 * Actual boot information structure.
 * Note: it should be less or equal than 2048 bytes (half page).
 */
struct bootinfo {
    uint32		bi_magic;	/* magic to validate */
    struct hdbiosinfo	bi_hdinfo[2];	/* hard disk parameters */
    char		bi_line[128];	/* argument line */
    capability		bi_loginsvr;	/* login server capability */
    char		bi_user[8];	/* user name */
    char		bi_passwd[8];	/* password */
    char		bi_offset[6];	/* magic for getport computation */
};

#endif /* __BOOTINFO_H__ */
