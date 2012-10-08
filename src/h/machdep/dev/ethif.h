/*	@(#)ethif.h	1.3	94/04/06 16:43:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Structure per multicast address
 */

typedef struct ether_multicast {
    char em_addr[6];				/* multicast address */
    struct ether_multicast	*em_next;	/* next on the list */
} ethmcast_t, *ethmcast_p;

/*
 * Structure per interface type
 */

typedef
struct hard_ethernet_interface_info {
	char *	hei_name;		/* for pretty printing */
	int	hei_nif;
	int	(*hei_alloc)(/* hei_nif */);
	int	(*hei_init)(/* hard_ifno, soft_ifno, &ifaddr */);
	void	(*hei_send)(/* ifno, pkt */);		
	int	(*hei_setmc)(/* ifno, *addr */);
	int	(*hei_stop)(/* ifno */);
} hei_t,*hei_p;

/*
 * structure per interface
 */
typedef
struct soft_ethernet_interface_info {
	char	sei_ifaddr[6];		/* Interface Ethernet Address */
	int	sei_nsndpkt;		/* # packets in the send ring */
	int	sei_nrcvpkt;		/* # packets in the receive ring */
	hei_p	sei_hei;		/* Hardware type */
	int	sei_ifno;		/* sequence number of that type */
	ethmcast_p sei_mcast;		/* List of multicast addresses */
} sei_t, *sei_p;
