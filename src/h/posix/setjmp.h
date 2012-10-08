/*	@(#)setjmp.h	1.4	94/04/06 16:53:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Posix-conformant definitions for setjmp() etc. */

#ifndef __SETJMP_H__
#define __SETJMP_H__

#if defined(__STDC__) && defined(__ACK)
/* In a jmp_buf, there is room for:  1 mask (long), 1 flag (int) and 3
 * pointers (stack-pointer, local base and program-counter). This may be
 * too big, but that doesn't matter. It could also be too small, when
 * sigset_t is larger than a long.  The fields is in the structure have no
 * meaning, they just get the size right.
 * The identifier __setjmp has a special meaning to the compiler.
 */

typedef struct {
	long __mask;
	int __flag;
	void (*__pc)(void);
	void *__sp;
	void *__lb;
} jmp_buf[1];

int     __setjmp(jmp_buf _env, int _savemask);

#define setjmp(env)     __setjmp(env, 0)
void    longjmp(jmp_buf _env, int _val);

#if defined(_POSIX_SOURCE)
typedef jmp_buf sigjmp_buf;
#define sigsetjmp(env, savemask)        __setjmp(env, savemask)
int     siglongjmp(sigjmp_buf _env, int _val);
#endif

#else

/*
** The typedefs for jmp_buf and sigjmp_buf are compiler-dependent.
** The definition for the specific compiler is found in
** h/posix/machdep/<arch>/_setjmp.h but is referenced as _setjmp.h
** so h/posix/machdep/<arch> must be in the include path for programs
** that use <setjmp.h>.
*/

#include "_setjmp.h"

#include "_ARGS.h"

/* Function declarations -- these aren't architecture-dependent */

int setjmp _ARGS((jmp_buf _env));
void longjmp _ARGS((jmp_buf _env, int _val));

#if defined(_POSIX_SOURCE)
typedef jmp_buf sigjmp_buf;	/* wrong, probably */

int sigsetjmp _ARGS((sigjmp_buf _env, int _savemask));
void siglongjmp _ARGS((sigjmp_buf _env, int _val));
#endif

#endif /* __STDC__ && ACK */

#endif /* __SETJMP_H__ */
