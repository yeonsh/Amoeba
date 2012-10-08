/*	@(#)alloca.h	1.1	94/05/17 10:29:44 */
#if !defined(__ALLOCA_H__)
#define	__ALLOCA_H__

#if defined(sparc)
#define	alloca(x)	__builtin_alloca(x)
#endif

#endif /* __ALLOCA_H__ */
