/*	@(#)rarp.h	1.1	91/04/10 10:52:37 */
/*
 * See RFC 903
 */
#define EP_RARP		0x8035
#define EP_IP		0x800

#define RARPBUFSIZE	6
#define NRARPMAP	50

struct rarp_pkt {
	char	rarp_dstaddr[6];
	char	rarp_srcaddr[6];
	uint16	rarp_ethertype;
	uint16	rarp_hrd;	/* 1 for Ethernet */
	uint16	rarp_pro;	/* EP_IP */
	char	rarp_hln;	/* 6 for Ethernet */
	char	rarp_pln;	/* 4 for IP */
	uint16	rarp_op;	/* see below */
	char	rarp_sha[6];
	char	rarp_spa[4];
	char	rarp_tha[6];
	char	rarp_tpa[4];
};

#define RARP_REQ	1	/* ARP request */
#define RARP_REP	2	/* ARP reply */
#define RARP_REVREQ	3	/* Reverse request */
#define RARP_REVREP	4	/* Reverse reply */
