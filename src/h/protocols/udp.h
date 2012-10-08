/*	@(#)udp.h	1.1	91/04/10 10:53:05 */
/*
 * UDP header, see RFC 768
 */

struct udp_hdr {
	uint16	udp_srcport;
	uint16	udp_dstport;
	uint16	udp_length;
	uint16	udp_chksum;
};
