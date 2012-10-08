/*	@(#)flgrp_marsh.h	1.2	96/02/27 14:02:01 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef _FLGRP_MARSHALL_H_
#define	_FLGRP_MARSHALL_H_

char *grp_unmarshall_uint16 _ARGS((char *p, char *e, g_index_t *i ));
char *grp_marshall_uint16   _ARGS((char *p, char *e, g_index_t *i ));

char *grp_marshall_result   _ARGS((char *p, char *e, group_p g, header_p hdr));
char *grp_unmarshall_result _ARGS((char *p, char *e, int bigend, group_p g));

char *grp_marshallgroup     _ARGS((char *p, char *e, group_p g,
				   g_index_t new, header_p hdr));
char *grp_unmarshallgroup   _ARGS((char *p, char *e, int bigend, group_p g));

char *grp_unmarshallmem	    _ARGS((char *p, char *e, int bigend, group_p g,
				   member_p mem));

f_size_t grp_max_marshall_size _ARGS((group_p g));

#endif /* _FLGRP_MARSHALL_H_ */
