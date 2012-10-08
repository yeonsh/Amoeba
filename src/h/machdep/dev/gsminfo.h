/*	@(#)gsminfo.h	1.2	94/04/06 16:44:18 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Shared memory info
 */

typedef struct sm_info {
	long	smi_devaddr;	/* Start address of device */
	int	smi_vec;	/* Interrupt vector */
	int	smi_maxaddr;	/* highest address on this network */
	int	smi_npkt;	/* Number of packet for the device */
	int	smi_pktsize;	/* size of packet */
	int	(*smi_getaddr)( /* smi_getarg, addr */ );
	long	smi_getarg;
	char *	(*smi_memalloc)( /* size */ );		/* local allocation */
	long	(*smi_addrconv)( /* kernel address */ );
	int 	smi_nnode;	/* number of nodes on the network */
	int	*smi_networkspec;	/* nodes on this network */
} sminfo_t, *sminfo_p;
