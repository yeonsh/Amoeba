/*	@(#)map.h	1.5	96/02/16 15:44:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define VME32BASE	0x00000000
#define VME32SIZE	0xFA000000
#define VME24BASE	0xFC000000
#define VME24SIZE	0x01000000
#define OBMEMBASE	0x00000000

#define IVECBASE	OBMEMBASE

#ifndef FLIP

#define BUSADDR(x)      force30_ether_addr_conv((phys_bytes) (x))

#endif /* FLIP */

#define KERNELSTART	((vir_bytes) IVECBASE)		/* kernel starts here */
#define KERNELSP	0x0010000
#define KERNELSPBOT	0x0002000

/* Convert virtual address to physical address */
#define	VTOP(x)		((phys_bytes) (x))
#define	PTOV(x)		((vir_bytes) (x))

#define PB_START	((struct printbuf *) 0x0000400)
#define PB_SIZE		0x1c00

#ifndef FLIP
/*
 * defines to make Ethernet work
 */
#define BUS2BIG
#define LOGNTRANS	3
#define LOGNRECV	4

#endif /* FLIP */

/*
 * OB device addresses
 */

#define DVADR_UA68562	0xFF802000
#define DVADR_MB87031	0xFF803400
#define DVADR_PT68230	0xFF800C00
#define	DVADR_RTC62421	0xFF803000	/* Real-time clock */

#ifdef PROFILE
#define DVADR_PT2_68230	0xFF800E00
#endif /* PROFILE */

#ifndef FLIP

#define DVADR_LANCE	0xFEF80000

#endif /* FLIP */
