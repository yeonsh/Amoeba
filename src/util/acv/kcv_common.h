/*	@(#)kcv_common.h	1.1	96/02/27 12:42:08 */
#ifndef KCV_COMMON_H
#define KCV_COMMON_H

#ifdef DEBUG
#define dprintf(list)   printf list
#else
#define dprintf(list)   {}
#endif

extern void convert();
extern void fatal();
extern void fatalperror();
extern void usage();
extern void writefile();

#endif
