/*	@(#)debug.h	1.1	91/11/20 13:08:03 */
/*
server/ip/debug.h
*/

#ifndef __SERVER__IP__DEBUG_H__
#define __SERVER__IP__DEBUG_H__

extern mutex ip_debug_mutex;

#define where() { mu_lock(&ip_debug_mutex); \
	printf("%s, %d: ", __FILE__, __LINE__); }

#define endWhere() mu_unlock(&ip_debug_mutex)

#endif /* __SERVER__IP__DEBUG_H__ */
