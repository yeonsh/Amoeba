/*	@(#)ip.h	1.1	91/04/10 10:52:22 */
/*
 * IP header, see RFC791
 */

struct ip_hdr {
	char	ip_vrsihl;		/* Version and Internet Header Length */
	char	ip_tos;			/* Type of Service                    */
	uint16	ip_totallength;		/* Length of datagram                 */
	uint16	ip_identification;	/* Identification for reassembly      */
	uint16	ip_fragoffset;		/* Fragment offset and flags          */
	char	ip_ttl;			/* Time to Live                       */
	char	ip_protocol;		/* Superior protocol                  */
	uint16	ip_checksum;		/* header checksum                    */
	long	ip_source;		/* Source IP address                  */
	long	ip_destination;		/* Destination IP address             */
};

/*
 * ip_vrsihl
 */
#define MK_VRSIHL(version, length)	(((version)<<4)|((length)>>2))
#define DEF_VRSIHL	MK_VRSIHL(4, sizeof(struct ip_hdr))

/*
 * ip_tos
 */
#define IP_TOS_PRC_NC		7
#define IP_TOS_PRC_INC		6
#define IP_TOS_PRC_CRITIC	5
#define IP_TOS_PRC_FLSHOVR	4
#define IP_TOS_PRC_FLASH	3
#define IP_TOS_PRC_IMMEDIATE	2
#define IP_TOS_PRC_PRIORITY	1
#define IP_TOS_PRC_ROUTINE	0

#define IP_TOS_LOWDELAY		8
#define IP_TOS_HIGHTHRU		16
#define IP_TOS_HIGHRELIAB	32

/*
 * ip_fragoffset
 */
#define IP_FRG_DF		0x4000
#define IP_FRG_MF		0x2000

/*
 * ip_protocol
 */
#define IP_PROTO_ICMP		1
#define IP_PROTO_IGMP		2
#define IP_PROTO_GGP		3
#define IP_PROTO_ST		5
#define IP_PROTO_TCP		6
#define IP_PROTO_UCL		7
#define IP_PROTO_EGP		8
#define IP_PROTO_IGP		9
#define IP_PROTO_BBN		10
#define IP_PROTO_NVPII		11
#define IP_PROTO_PUP		12
#define IP_PROTO_ARGUS		13
#define IP_PROTO_EMCON		14
#define IP_PROTO_XNET		15
#define IP_PROTO_CHAOS		16
#define IP_PROTO_UDP		17
#define IP_PROTO_MUX		18
#define IP_PROTO_DCN		19
#define IP_PROTO_HMP		20
#define IP_PROTO_PRM		21
#define IP_PROTO_XNS		22
#define IP_PROTO_TRUNK1		23
#define IP_PROTO_TRUNK2		24
#define IP_PROTO_LEAF1		25
#define IP_PROTO_LEAF2		26
#define IP_PROTO_RDP		27
#define IP_PROTO_IRTP		28
#define IP_PROTO_ISOTP4		29
#define IP_PROTO_NETBLT		30
#define IP_PROTO_MFENSP		31
#define IP_PROTO_MERIT		32
#define IP_PROTO_SEP		33
#define IP_PROTO_CFTP		62
#define IP_PROTO_SATNET		64
#define IP_PROTO_MITSUBNET	65
#define IP_PROTO_RVD		66
#define IP_PROTO_IPPC		67
#define IP_PROTO_SATMON		69
#define IP_PROTO_IPCV		71
#define IP_PROTO_BRSATMON	76
#define IP_PROTO_WBMON		78
#define IP_PROTO_WBEXPAK	79
