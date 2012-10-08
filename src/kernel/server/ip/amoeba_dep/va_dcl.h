/*	@(#)va_dcl.h	1.1	91/11/25 12:58:54 */
/*
va_dcl.h

Created July 25, 1991 by Philip Homburg
*/

#ifndef VA_DCL_H
#define VA_DCL_H

#ifdef __STDC__
#include "stdarg.h"
#define VA_LIST		va_list
#define VA_ALIST
#define VA_DCL
#define VA_START( ap, last )	va_start( ap, last )
#define VA_END		va_end
#define VA_ARG		va_arg
#else
#include "varargs.h"
#define VA_LIST		va_list
#define VA_ALIST	, va_alist
#define VA_DCL		va_dcl
#define VA_START( ap, last )	va_start( ap )
#define VA_END		va_end
#define VA_ARG		va_arg
#endif	/* __STDC__ */

#endif /* VA_DCL_H */
