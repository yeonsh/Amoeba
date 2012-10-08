/*	@(#)stdlib.h	1.1	91/04/10 14:49:30 */
#ifndef __STDLIB_H__
#define __STDLIB_H__

#if     !defined(_SIZE_T)
#define _SIZE_T
typedef unsigned int    size_t;         /* type returned by sizeof */
#endif  /* _SIZE_T */

#if     !defined(_WCHAR_T)
#define _WCHAR_T
typedef char    wchar_t;
#endif  /* _WCHAR_T */

#include "../generic/stdlib_gen.h"

#endif /* __STDLIB_H__ */
