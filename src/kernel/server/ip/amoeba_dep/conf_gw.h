/*	@(#)conf_gw.h	1.1	91/11/25 12:58:14 */
/*
default_gw.h

Created Sept 25, 1991 by Philip Homburg
*/

#ifndef AMOEBA_DEP__DEFAULT_GW_H
#define AMOEBA_DEP__DEFAULT_GW_H

typedef struct gw_list
{
	char ipaddr[4];
	u32_t priority;
} gw_list_t;

extern gw_list_t default_wg_list[];

#endif /* AMOEBA_DEP__DEFAULT_GW_H */
