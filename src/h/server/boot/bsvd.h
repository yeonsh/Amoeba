/*	@(#)bsvd.h	1.2	94/04/06 16:58:28 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __BSVD_H__
#define __BSVD_H__

/*
 *	The header of the vdisk for the bootserver
 */
struct bsvd_head {
    char magic[8];	/* null terminated */
    capability binary;	/* Bullet file containing bootserver binary */
    capability supercap;/* Getport of bootserver */
    /*
     *	Before this point, everything is byteorder independent,
     *	beyond it, there are only long integers.
     *	The marshalling macros depend on this.
     */
    int32 l2vbl;	/* Not used currently */
    struct bsvd_file {	/* See below */
	int32 size;
	int32 block1;
    } proccaps, conf;
};

#define BOOT_MAGIC	"-boot-"	/* The magic string */
#define L2BLOCK		9
#define BLOCKSIZE	(1 << L2BLOCK)
#define PROCCAPSIZE	(40*BLOCKSIZE)	/* default # bytes for proccaps file */

/*
 *	The disk basically consists of three parts:
 *	At block zero is a bsvd_head, marshaled with
 *	the macros below. It contains the position
 *	of the other two as a (size, position) pair.
 *	The sizes are actual sizes, since the reserved
 *	space can be computed.
 *	The second part is storage for the boot server.
 *	It's format is left unspecified here.
 *	The third part is the configurationfile.
 */

/*
 *	Marshaling macros; watch out for side-effects.
 *	You need to include byteorder.h yourself to use this.
 */
#ifdef lint
#define DEC_BSVDP(bsvdp)	((bsvdp)->conf.size = 3)
#define ENC_BSVDP(bsvdp)	((bsvdp)->conf.size = 3)
#else /* lint */
#define DEC_BSVDP(bsvdp) \
	do { \
	    int32 *_ii = (int32 *) ((struct bsvd_head *) ((bsvdp) + 1)); \
	    while (_ii-- > &(bsvdp)->l2vbl) \
		dec_l_be(_ii); \
	} while (0)

#define ENC_BSVDP(bsvdp) \
	do { \
	    int32 *_ii = (int32 *) ((struct bsvd_head *) ((bsvdp) + 1)); \
	    while (_ii-- > &(bsvdp)->l2vbl) \
		enc_l_be(_ii); \
	} while (0)
#endif

#endif /* __BSVD_H__ */
