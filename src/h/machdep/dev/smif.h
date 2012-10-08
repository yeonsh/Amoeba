/*	@(#)smif.h	1.2	94/04/06 16:46:17 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Structure per interface type
 */

typedef
struct hw_sm_info {
	char *	hmi_name;		/* for pretty printing */
	int	hmi_nif;
	int 	(*hmi_alloc)(/* hei_nif */);
	int	(*hmi_init)(/* hard_ifno, soft_ifno, &ifaddr */);
	void	(*hmi_send)(/* ifno, pkt, &dst, proto */);		
	void	(*hmi_broadcast)(/* ifno, pkt, proto */);		
} hmi_t,*hmi_p;

/*
 * structure per interface
 */
typedef
struct sw_sm_info {
	int	smi_ifaddr;		/* Interface sm Address */
	hmi_p	smi_hmi;		/* Hardware type */
	int	smi_ifno;		/* sequence number of that type */
} smi_t, *smi_p;
