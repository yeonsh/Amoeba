/*	@(#)memaccess.h	1.3	94/04/06 15:41:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef _MEM_ACCESS_H
#define _MEM_ACCESS_H

#if defined(__STDC__) || defined(lint)

#define mem_get_long(/*volatile long* */p)		    (*(p))
#define mem_put_long(/*volatile long* */p, /* long */ v) ((void)(*(p) = v))
#define mem_OR_long( /*volatile long* */p, /* long */ v) ((void)(*(p) |= v))
#define mem_AND_long(/*volatile long* */p, /* long */ v) ((void)(*(p) &= v))

#define mem_get_short(/*volatile short* */p)		    (*(p))
#define mem_put_short(/*volatile short* */p, /* short */ v) ((void)(*(p) = v))
#define mem_OR_short( /*volatile short* */p, /* short */ v) ((void)(*(p) |= v))
#define mem_AND_short(/*volatile short* */p, /* short */ v) ((void)(*(p) &= v))

#define mem_get_byte( /*volatile char * */p)		    (*(p))
#define mem_put_byte( /*volatile char * */p, /* char */ v)  ((void)(*(p) = v))
#define mem_OR_byte(  /*volatile char * */p, /* char */ v)  ((void)(*(p) |= v))
#define mem_AND_byte( /*volatile char * */p, /* char */ v)  ((void)(*(p) &= v))

#define mem_sync()	{}

#if defined(lint)
#define volatile /* volatile */
#endif

#else

#define volatile /* volatile */

extern long  mem_get_long( /* long *p		*/	);
extern void  mem_put_long( /* long *p, long val */	);
extern void  mem_OR_long(  /* long *p, long val */	);
extern void  mem_AND_long( /* long *p, long val */	);

extern short mem_get_short( /* short *p		   */	);
extern void  mem_put_short( /* short *p, short val */	);
extern void  mem_OR_short(  /* short *p, short val */	);
extern void  mem_AND_short( /* short *p, short val */	);

extern char  mem_get_byte(  /* char *p		   */	);
extern void  mem_put_byte(  /* char *p, char val   */	);
extern void  mem_OR_byte(   /* char *p, char val   */	);
extern void  mem_AND_byte(  /* char *p, char val   */	);

extern void mem_sync();
#endif /* __STDC__ || lint */

#endif /* _MEM_ACCESS_H */
