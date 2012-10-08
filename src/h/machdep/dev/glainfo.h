/*	@(#)glainfo.h	1.5	94/04/06 16:44:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Lance info
 */

typedef
struct la_info {
	long	lai_devaddr;
	intrinfo lai_intrinfo;
	int	lai_csr3;	/* CSR3 initialization value */
	int	lai_logrcv;	/* log of # of receive entries */
	int	lai_logtmt;	/* log of # of transmit entries */
	int	lai_lalogtmt;	/* size pkt pool lance driver */
	int	lai_dont_chain;	/* don't use hardware chaining */
	int	lai_min_chain_head_size;
	int	(*lai_getaddr)( /* lai_getarg, addr */ );
	long	lai_getarg;
	char *	(*lai_memalloc)( /* size */ );
	uint32	(*lai_addrconv)( /* kernel address */ );
} lai_t, *lai_p;
