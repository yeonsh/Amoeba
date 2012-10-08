/*	@(#)des-private.h	1.1	91/11/18 16:25:38 */
#include <module/des.h>
#ifndef DES_PRIVATE
#define DES_PRIVATE 1

#ifdef mc68000
#define BIG_ENDIAN 1
#endif

#ifdef sparc
#define	BIG_ENDIAN 1
#endif

#ifdef interdata
#define BIG_ENDIAN 1
#endif

/* turns out this is useless, its too late, and not seen everywhere .. kre */
#if (!__STDC__)
#define const
#endif

#define copy_N(from,to,type) (*((type*)&(to)) = *((type*)&(from)))
#define copy8(from,to) copy_N(from,to,C_Block)
#define copy4(from,to) copy_N(from,to,des_u_long)
#define lval_N(from,type) (*((type*)&(from)))
#define val4(variable) lval_N(variable,des_u_long)

#endif
