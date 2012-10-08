/*	@(#)stdio.h	1.5	96/02/27 10:34:48 */
/*
 * stdio.h - input/output definitions
 *
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#if	!defined(_STDIO_H)
#define	_STDIO_H

#ifdef __STDC__
#include "__stdarg.h"  /* For when we need the ___va_list definition */
#endif

/*
 * Focus point of all stdio activity.
 */
typedef struct __iobuf {
	int		_count;
	int		_fd;
	int		_flags;
	int		_bufsiz;
	unsigned char	*_buf;
	unsigned char	*_ptr;
	long		_mu;
	/* This type should really be mutex, but we can't force users to
	 * include another file before this one.  Moreover, ANSI disallows
	 * our mutex type being automatically defined this way.
	 */
} FILE;

#define	_IOFBF		0x000
#define	_IOREAD		0x001
#define	_IOWRITE	0x002
#define	_IONBF		0x004
#define	_IOMYBUF	0x008
#define	_IOEOF		0x010
#define	_IOERR		0x020
#define	_IOLBF		0x040
#define	_IOREADING	0x080
#define _IOWRITING	0x100
#define	_IOAPPEND	0x200

/* The following definitions are also in <unistd.h>. They should not
 * conflict.
 */
#define	SEEK_SET	0
#define	SEEK_CUR	1
#define	SEEK_END	2

#define	stdin		(&__stdin)
#define	stdout		(&__stdout)
#define	stderr		(&__stderr)

#define	BUFSIZ		1024
#define	EOF		(-1)

#ifdef __STDC__
#define	NULL		((void *)0)
#else
#ifndef NULL
#define NULL       0
#endif
#endif /* __STDC__ */

#define	FOPEN_MAX	20

#if	defined(__BSD4_2) || defined(AMOEBA)
#define	FILENAME_MAX	255
#else
#define	FILENAME_MAX	14
#endif
#define	TMP_MAX		999
#define	L_tmpnam	(sizeof("/tmp/") + 15)
#define	L_ctermid	9

typedef long int	fpos_t;

#if	!defined(_SIZE_T)
#define	_SIZE_T
typedef unsigned int	size_t;		/* type returned by sizeof */
#endif	/* _SIZE_T */

extern FILE	*__iotab[FOPEN_MAX];
extern FILE	__stdin, __stdout, __stderr;

#include "_ARGS.h"

int	remove	 _ARGS((const char *_filename));
int	rename	 _ARGS((const char *_old, const char *_new));
FILE	*tmpfile _ARGS((void));
char	*tmpnam	 _ARGS((char *_s));
int	fclose	 _ARGS((FILE *_stream));
int	fflush	 _ARGS((FILE *_stream));
FILE	*fopen	 _ARGS((const char *_filename, const char *_mode));
FILE	*freopen _ARGS((const char *_filename, const char *_mode,
			FILE *_stream));
void	setbuf	 _ARGS((FILE *_stream, char *_buf));
int	setvbuf	 _ARGS((FILE *_stream, char *_buf, int _mode, size_t _size));
int	fprintf	 _ARGS((FILE *_stream, const char *_format, ...));
int	fscanf	 _ARGS((FILE *_stream, const char *_format, ...));
int	printf	 _ARGS((const char *_format, ...));
int	scanf	 _ARGS((const char *_format, ...));
int	sprintf	 _ARGS((char *_s, const char *_format, ...));
int	sscanf	 _ARGS((const char *_s, const char *_format, ...));
int	vfprintf _ARGS((FILE *_stream, const char *_format, ___va_list _arg));
int	vprintf	 _ARGS((const char *_format, ___va_list _arg));
int	vsprintf _ARGS((char *_s, const char *_format, ___va_list _arg));
int	fgetc	 _ARGS((FILE *_stream));
char	*fgets	 _ARGS((char *_s, int _n, FILE *_stream));
int	fputc	 _ARGS((int _c, FILE *_stream));
int	fputs	 _ARGS((const char *_s, FILE *_stream));
int	getc	 _ARGS((FILE *_stream));
int	getchar	 _ARGS((void));
char	*gets	 _ARGS((char *_s));
int	putc	 _ARGS((int _c, FILE *_stream));
int	putchar	 _ARGS((int _c));
int	puts	 _ARGS((const char *_s));
int	ungetc	 _ARGS((int _c, FILE *_stream));
size_t	fread	 _ARGS((void *_ptr, size_t _size,
			size_t _nmemb, FILE *_stream));
size_t	fwrite	 _ARGS((const void *_ptr, size_t _size,
			size_t _nmemb, FILE *_stream));
int	fgetpos	 _ARGS((FILE *_stream, fpos_t *_pos));
int	fseek	 _ARGS((FILE *_stream, long _offset, int _whence));
int	fsetpos	 _ARGS((FILE *_stream, fpos_t *_pos));
long	ftell	 _ARGS((FILE *_stream));
void	rewind	 _ARGS((FILE *_stream));
void	clearerr _ARGS((FILE *_stream));
int	feof	 _ARGS((FILE *_stream));
int	ferror	 _ARGS((FILE *_stream));
void	perror	 _ARGS((const char *_s));

int __fillbuf	 _ARGS((FILE *_stream));
int  __flushbuf	 _ARGS((int _c, FILE *_stream));

#define	__getchar()	getc(stdin)
#define	__putchar(c)	putc(c,stdout)
#define	__getc(p)	(--(p)->_count >= 0 ? (int) (*(p)->_ptr++) : \
				__fillbuf(p))
#define	__putc(c, p)	(--(p)->_count >= 0 ? \
			 (int) (*(p)->_ptr++ = (c)) : \
			 __flushbuf((c),(p)))

#define	__feof(p)	(((p)->_flags & _IOEOF) != 0)
#define	__ferror(p)	(((p)->_flags & _IOERR) != 0)
#define __clearerr(p)	((p)->_flags &= ~(_IOERR|_IOEOF))

#define getchar()	__getchar()
#define putchar(c)	__putchar(c)
#define getc(p)		__getc(p)
#define putc(c, p)	__putc(c, p)
#define feof(p)		__feof(p)
#define ferror(p)	__ferror(p)
#define clearerr(p)	__clearerr(p)

#if defined(__BSD4_2)||defined(__USG)||defined(_POSIX_SOURCE)||defined(AMOEBA)
int	fileno _ARGS((FILE *_stream));
FILE   *fdopen _ARGS((int _fildes, const char *_type));

#define	__fileno(stream) ((stream)->_fd)
#define fileno(stream)	 __fileno(stream)	
#endif	/* __BSD4_2 || __USG || _POSIX_SOURCE || AMOEBA */

#ifdef _POSIX2_SOURCE
FILE   *popen _ARGS((const char *_command, const char *_mode));
#endif

#endif	/* _STDIO_H */
