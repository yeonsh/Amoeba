/*	@(#)icmp.h	1.3	96/02/27 10:36:56 */
/*
server/ip/gen/icmp.h
*/

#ifndef __SERVER__IP__GEN__ICMP_H__
#define __SERVER__IP__GEN__ICMP_H__

#define ICMP_MIN_HDR_LEN	4

#define ICMP_TYPE_ECHO_REPL	0
#define ICMP_TYPE_DST_UNRCH	3
#define ICMP_TYPE_SRC_QUENCH	4
#	define ICMP_NET_UNRCH			0
#	define ICMP_HOST_UNRCH			1
#	define ICMP_PROTOCOL_UNRCH		2
#	define ICMP_PORT_UNRCH			3
#	define ICMP_FRAGM_AND_DF		4
#	define ICMP_SOURCE_ROUTE_FAILED		5
#define ICMP_TYPE_REDIRECT	5
#	define ICMP_REDIRECT_NET		0
#	define ICMP_REDIRECT_HOST		1
#	define ICMP_REDIRECT_TOS_AND_NET	2
#	define ICMP_REDIRECT_TOS_AND_HOST	3
#define ICMP_TYPE_ECHO_REQ	8
#define ICMP_TYPE_ROUTER_ADVER	9
#define ICMP_TYPE_ROUTE_SOL	10
#define ICMP_TYPE_TIME_EXCEEDED	11
#	define ICMP_TTL_EXC			0
#	define ICMP_FRAG_REASSEM		1
#define ICMP_TYPE_PARAM_PROBLEM	12
#define ICMP_TYPE_TS_REQ	13
#define ICMP_TYPE_TS_REPL	14
#define ICMP_TYPE_INFO_REQ	15
#define ICMP_TYPE_INFO_REPL	16

#endif /* __SERVER__IP__GEN__ICMP_H__ */

/*
 * $PchHeader: /mount/hd2/minix/include/net/gen/RCS/icmp.h,v 1.4 1994/12/14 21:47:44 philip Exp $
 */
