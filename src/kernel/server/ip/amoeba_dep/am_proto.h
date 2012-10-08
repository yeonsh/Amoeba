/*	@(#)am_proto.h	1.1	91/11/25 12:57:23 */
/*
am_proto.h
*/

#include "sys/kernelf_wo_proto.h"

/* am_eth.c */
void eth_init_ehook_names ARGS(( void ));
void eth_set_ehook_name ARGS(( int minor, char *ehook_name ));

/* nw_task.c */
void usage ARGS(( void ));
#if AM_KERNEL
void tcpipinit ARGS(( void ));
#else
int main ARGS (( int argc, char *argv[] ));
#endif

/* sr.c */
void sr_init_cap_names ARGS(( void ));
void sr_set_cap_name ARGS(( int minor, char *cap_name ));
void sr_enable_linger_right ARGS(( void ));
