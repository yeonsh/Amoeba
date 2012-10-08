/*	@(#)stdlib.h	1.1	93/03/03 13:04:25 */
#ifndef __STDLIB_H__
#define __STDLIB_H__

#if !defined(__sys_stdtypes_h) && !defined(TYPES_H) && !defined(_TYPES_)
#define __sys_stdtypes_h	/* prevent multiple inclusion (SunOS 4.1.1) */
#define TYPES_H			/* prevent multiple inclusion (SunOS 4.0.3) */
#define _TYPES_			/* prevent multiple inclusion (SunOs 4.0.3) */


#if     !defined(_SIZE_T)
#define _SIZE_T
typedef unsigned int    size_t;         /* type returned by sizeof */
#endif  /* _SIZE_T */

#if     !defined(_WCHAR_T)
#define _WCHAR_T
typedef char    wchar_t;
#endif  /* _WCHAR_T */

#if	!defined(_SIGSET_T)
#define _SIGSET_T
typedef int sigset_t;			/* signal mask */
#endif	/* _SIGSET_T */

#if	!defined(_TIME_T)
#define _TIME_T
typedef long time_t;
#endif	/* _TIME_T */

#if	!defined(_PID_T)
#define _PID_T
typedef int pid_t;                      /* process id */
#endif	/* _PID_T */

#endif /* __sys_stdtypes_h */

#include "../generic/stdlib_gen.h"

#endif /* __STDLIB_H__ */
