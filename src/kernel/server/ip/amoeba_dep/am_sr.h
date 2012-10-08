/*	@(#)am_sr.h	1.1	91/11/25 12:57:31 */
/*
am_sr.h

Created Sept 30, 1991 by Philip Homburg
*/

#ifndef AM_SR_H
#define AM_SR_H

/* Prototypes */

void sr_init_cap_names ARGS(( void ));
void sr_set_cap_name ARGS(( int minor, char *cap_name ));
void sr_enable_linger_right ARGS(( void ));

#endif /* AM_SR_H */
