/*	@(#)vc_int.h	1.4	94/04/07 10:20:54 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
vc_int.h

Created August 5, 1991 by Philip Homburg
*/

void _vc_zap_cb _ARGS(( struct vc *vc, int which ));
void _vc_die _ARGS(( struct vc *vc, int which ));
void _vc_server _ARGS(( char *arg, int argsize ));
void _vc_client _ARGS(( char *arg, int argsize ));
void _vc_my_p _ARGS(( int32 level, ... ));

void _vc_close_client _ARGS((struct vc *vc));
void _vc_close_server _ARGS((struct vc *vc));
