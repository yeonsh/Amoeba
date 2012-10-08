/*	@(#)bfile.c	1.3	94/04/07 14:31:50 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "ail.h"
#include <errno.h>

#ifdef __STDC__
#include <string.h>

/* in case of flakey string.h header: */
extern char *strerror();

char *
error_string(err)
int err;
{
	return(strerror(err));
}
#else
extern int errno;	/* Why don't they put it where it belongs?	*/
extern int sys_nerr;	/*  "    "     "   "  "    "   "    "		*/
extern char *sys_errlist[];	/*"    "   "  "    "   "    "		*/

char *
error_string(err)
int err;
{
	return (err < sys_nerr) ? sys_errlist[errno] : "meta-error";
}
#endif /* __STDC__ */

#ifndef GEMDOS
#define DIRSEP	'/'
#else
#define DIRSEP	'\\'
#endif

/*
 *	The backpatch file package provides these primitives:
 *
 *	- OpenFile(char *name)
 *		Open a file, return a 'handle' for it.
 *		This involves no interaction with a real
 *		filesystem.
 *		The name is stored in an array, so that
 *		ail finds out when a file is written twice.
 *
 *	- BackPatch(handle)
 *		Get a new handle within an opened file.
 *		Text written on this handle will be placed
 *		between the last and the next character written
 *		on the old handle. The old handle itself may
 *		be obtained from OpenFile(), or BackPatch().
 *		BackPatches don't need to be closed by any means.
 *
 *	- CloseFile(handle)
 *		Write the complete file to a real filesystem.
 *		All BackPatches are inserted. The filename on
 *		the real filesystem is the original name, pre-
 *		fixed with the current output directory.
 *
 *	- ListFiles
 *		This is an ail-style generator that lists the
 *		names of the files that ail generated thus far.
 *
 *	- IndentChange(handle, offset)
 *		We keep track of an indentation level here as
 *		well. An indentation level corresponds to four
 *		spaces, so two levels makes a tab.
 *		The initial level is zero for a file, and the
 *		file's indentation level for backpatches.
 *		Call IndentChange with a positive offset to
 *		increase the level, and a negative one to
 *		decrease it
 *
 *	- Indent(handle)
 *		Add a newline and the correct amount of spaces
 *		and tabs, as indicated by its current indentation
 *		level.
 *
 *	- AddBuf(handle, char *buffer, int n)
 *		Add n characters from the buffer to the handle.
 *
 *	- AddChar(handle, char ch)
 *		Add the character 'ch' to the handle.
 *
 *	The primitives to write on a handle are on myprintf.c.
 *	They substitute STDIO's fprintf, which writes on a FILE
 *	pointer instead of on a handle.
 *
 *	Special handles are {OUT,ERR}_HANDLE, which correspond
 *	with stdout and stderr. They are negative. For obvious
 *	reasons, one cannot open, close or backpatch these handles.
 *	Less obvious is that one _can_ get and set the indent
 *	of these handles. (I simply need it - the diagnostics
 *	may print a type definition, and structs are indented)
 *
 *	When the file is eventually written, we write the file
 *	using a write on a unix filehandle, or stdio's fwrite
 *	on a FILE *. This depends on the compile-time switch
 *	USE_STDIO.
 */

/* Max # open backpatches and files	*/
#ifndef MAX_OPEN
#define MAX_OPEN 10
#endif

/*
 *	Size used to allocate blocks.
 *	If this is fairly low, I'm still debugging.
 */
#ifndef CHUNK_SIZE
#define CHUNK_SIZE	( 20 * 1024 )
#endif

/*
 *	The first time a block is allocated, we try a small number,
 *	because most backpatches are quite small in practice.
 */
#ifndef LITTLE_CHUNK
#define LITTLE_CHUNK	1024
#endif

/*
 *	Which basic I/O system we use: stdio or unix.
 *
 *	The stdio version is to ensure portability,
 *	but it may even be faster than the unix version
 *	if your fwrite() is clever enough to avoid
 *	copying when the requested size is bigger than
 *	BUFSIZ. You end up using four levels of I/O this
 *	way: myprintf, bfile, stdio, systemcalls, where
 *	on Amoeba, the systemcalls are of course implemented
 *	by the unix compatibility library, on top of some
 *	runtime choosen RPC-interface...
 *
 *	Ail typically calls the write routine several
 *	times for small portions, and then one time with
 *	a big buffer. The server mainloop for instance,
 *	consists of four blocks, and one big block extending
 *	from 'getreq' until eof.
 *	Assembling the first blocks into one may improve
 *	the speed writing little files like clientstubs.
 */

#ifdef __STDC__
#define USE_STDIO
#endif

#ifdef	USE_STDIO
typedef FILE *io_d;				/* Basic handle-type	*/
#define STD_OUT		stdout			/* Handle for standard output */
#define STD_ERR		stderr			/* and diagnostics */
#define OPEN(name)	fopen(name, "w")	/* Open a file for writing */
#define OPEN_ERR	NULL			/* Value indicating failure */
#define WRITE(h, p, n)	(fwrite(p, 1, n, h) == n) /* Write some bytes */
#define CLOSE(h)	(fclose(h) == 0)	/* Close file, report success */
#else	/* USE_STDIO */
typedef int io_d;
#define STD_OUT		1
#define STD_ERR		2
#define OPEN(name)	open(name, O_TRUNC|O_WRONLY|O_CREAT, 0666)
#define OPEN_ERR	-1
#define WRITE(h, p, n)	mywrite(h, p, n)
#define CLOSE(h)	(close(h) >= 0)
#ifdef	SYSV		/* Hmm. What's the ansi or posix name for this? */
#include <sys/fcntl.h>
#else	/* SYSV */
#include <sys/file.h>
#endif	/* SYSV */
#endif	/* USE_STDIO */

/****************************************************************\
 *								*
 *	The datastructures used:				*
 *								*
 *	Text is kept in blocks of at most BL_SIZE chars.	*
 *	A block can also be a placeholder for a backpatch.	*
 *	In that case, it's size is -1.				*
 *	Blocks are malloced (and freed, hopefully).		*
 *								*
\****************************************************************/

struct block {		/* Where I store the file contents	*/
	struct block *bl_next;	/* Successor			*/
	int bl_size;		/* How many chars are here, -1	*/
	int bl_cnt;		/* Index of last written char	*/
	union {
	    char *u_text;	/* Where they are stored	*/
	    handle u_patch;	/* Or where they are stored :-)	*/
	} bl_u;
};
#define bl_text		bl_u.u_text
#define bl_patch	bl_u.u_patch

static char is_patch;	/* Backpatches claim to have this name	*/

/*
 *	A handle is really an index in the files-array.
 *	The pointer to the last block is only for speedup,
 *	though I don't know how much I win with this.
 */
static struct {		/* Descriptor for a file/backpatch	*/
	char *f_name;		/* NULL if unused		*/
	int f_indent;		/* Current indentation level	*/
	struct block *f_blks;	/* The file's contents or NULL	*/
	struct block *f_last;	/* The current block, if any	*/
} files[MAX_OPEN];

/*
 *	Indentation levels for standard files:
 */
static int err_indent = 0, out_indent = 0;

/*
 *	We want to remember which files we generated,
 *	to implement _list_files and to give an error
 *	when previous output gets overwritten.
 */
#ifndef MAX_GENER	/* Max # files to generate in one run	*/
#define MAX_GENER	200
#endif
static char *outputfiles[MAX_GENER];	/* Only reason for this limit	*/
static char **outputend = &outputfiles[0];

/*
 *	A boring macro:
 */
#define chkhandle(h)		assert(h >= -2 && h < MAX_OPEN)
/* #define chkhandle(h)		if (h < -2 || h >= MAX_OPEN) abort() */

/*
 *	And another one:
 */
#define ReleaseHandle(h)	(files[h].f_name = NULL)

/********************************************************\
 *							*
 *	The standard handles are written immediately,	*
 *	the other ones when the file is closed		*
 *							*
\********************************************************/

#ifndef USE_STDIO
/*
 *	This version of write() writes the requested number
 *	of bytes, and returns a boolean indicating success
 */
static
mywrite(h, buf, size)
    io_d h;
    char *buf;
    unsigned int size;
{
    while (size > 0) {
	int n;
	n = write(h, buf, size);
	if (n < 0) return No;
	assert(n <= size);	/* You never know	*/
	size -= n;
	buf += n;
    }
    return Yes;
} /* mywrite */
#endif /* USE_STDIO */

static void
WriteFile(fd, bp)	/* Virtual file is closed, let's get physical!	*/
    io_d fd;
    struct block *bp;
{
    while (bp != NULL) {
	if (bp->bl_size < 0) {		/* This is a backpatch	*/
	    chkhandle(bp->bl_patch);
	    assert(files[bp->bl_patch].f_name == &is_patch);
	    WriteFile(fd, files[bp->bl_patch].f_blks);
	    ReleaseHandle(bp->bl_patch);
	} else if (fd != OPEN_ERR) {	/* Write the block	*/
	    assert(bp->bl_cnt <= bp->bl_size);
	    assert(bp->bl_cnt >= 0);
	    errno = 0;
	    if (!WRITE(fd, bp->bl_text, bp->bl_cnt))
		fatal("Write failed\n");
	}

	{
	    /* Only the block needs to be freed	*/
	    register struct block *np;
	    np = bp->bl_next;
	    free((char *) bp);
	    bp = np;
	}
    }
} /* WriteFile */

void
CloseFile(h)		/* Close a file, write to file service	*/
    handle h;
{
    io_d fd;	/* File descriptor known by operating system	*/
    chkhandle(h);
    assert(files[h].f_name != NULL);		/* Unused !?	*/
    assert(files[h].f_name != &is_patch);	/* Close backp?	*/
    if (ErrCnt != 0 || (options & OPT_NOOUTPUT))
	fd = OPEN_ERR;			/* Mimic open failure	*/
    else {
	errno = 0;
	fd = OPEN(files[h].f_name);
	if (fd == OPEN_ERR) {
	    mypf(ERR_HANDLE, "%r (writing) %s: %s\n", files[h].f_name,
		 error_string(errno));
	}
    }
    WriteFile(fd, files[h].f_blks);	/* (Write and) release blocks	*/
    if (fd != OPEN_ERR) {
	errno = 0;
	if (!CLOSE(fd))
	    mypf(ERR_HANDLE, "%r Problems on close %s: %s\n",
		files[h].f_name, error_string(errno));
    }
    ReleaseHandle(h);
} /* CloseFile */

/*
 *	List the names of the outputfiles.
 */
/*ARGSUSED*/
void
ListFiles(args)
    struct gen *args;
{
    handle fp;
    char **namep, **last;
    /*
     *	If an identifier was passed, it's a
     *	filename, otherwise write to stdout.
     *	Make sure the name of the outputfile
     *	doesn't appear in its own file.
     */
    last = outputend;
    if (args == NULL) fp = OUT_HANDLE;
    else {
	assert(args->gn_name != NULL);
	fp = OpenFile(args->gn_name);
    }
    for (namep = &outputfiles[0]; namep < last; ++namep)
	mypf(fp, "%s\n", *namep);
    if (fp != OUT_HANDLE) CloseFile(fp);
} /* ListFiles */

/****************************************************************\
 *								*
 *	The administrativa					*
 *								*
\****************************************************************/

/*
 *	Add 'offset' to the current indent.
 *	The assert gets triggered every time you hit an indentation bug.
 */
void
IndentChange(h, offset)
    handle h;
    int offset;
{
    chkhandle(h);
    switch (h) {
    case ERR_HANDLE:
	if ((err_indent += offset) < 0) abort();
	return;
    case OUT_HANDLE:
	if ((out_indent += offset) < 0) abort();
	return;
    }
    files[h].f_indent += offset;
    if (files[h].f_indent < 0) {
	if (!ErrCnt) {
	    mypf(ERR_HANDLE, "%r Indentation BUG file %s; so far:\n",
							files[h].f_name);
	    /* This may fail, but we hit a bug anyway */
	    WRITE(STD_ERR, files[h].f_blks->bl_text, files[h].f_blks->bl_cnt);
	    assert(0);
	}
	files[h].f_indent = 0;
    }
} /* IndentChange */

/*
 *	Get a free entry in the files[] array.
 */
static handle
GetFreeHandle()
{
    handle h;
    for (h = 0; h < MAX_OPEN; ++h)
	if (files[h].f_name == NULL) break;
    if (h >= MAX_OPEN) fatal("Implementation limit; change MAX_OPEN\n");
    return h;
} /* GetFreeHandle */

/*
 *	Truncate the name of a file (they sometimes get verrrry long).
 *	This name contains a slash, and probably one or more dots.
 */
static void
TruncatePath(s)
    char *s;
{
    int x;	/* How many chars are too much	*/
    char *last_slash = NULL, *first_dot = NULL, *p;
    assert(MaxFilenameLen > 0);
    for (p = s; *p != '\0'; ++p) {
	if (*p == DIRSEP)
	    last_slash = p;
	if (last_slash != NULL && first_dot == NULL && *p == '.')
	    first_dot = p;
    }
    assert(*last_slash == DIRSEP);
    if (first_dot == NULL)	/* No dot in the filename!? Oh well,	*/
	first_dot = p;
    x = strlen(last_slash + 1) - MaxFilenameLen;
    if (x > 0) {
	p = first_dot - x;
	if (p <= last_slash) {
	    mypf(ERR_HANDLE, "%r cannot truncate %s\n", last_slash + 1);
	} else {
	    /* Truncate */
	    while (*p++ = *first_dot++)
		;
	}
    }
} /* TruncatePath */

/*
 *	Create a handle for file in the outputdirectory.
 *	This file will be written upon closing it.
 *	The name is not supposed to contain directory
 *	separators.
 *	If the option for backing up was set and a file
 *	by that name existed yet, move it to <filename>.BAK
 *	Of course, we don't check whether a file by _THAT_
 *	name existed.
 */
handle
OpenFile(name)
    char *name;
{
    handle h;
    char **p;
    int twice;
    char buf[1024];		/* Enough on Unix	*/

    assert(name != NULL);
    sprintf(buf, "%s%c%s", outdir, DIRSEP, name);
    if (MaxFilenameLen > 0) TruncatePath(buf);
    name = StrFind(buf);	/* Make it unique	*/
    /* Didn't we write this one before?	*/
    twice = No;
    for (p = outputfiles; p < outputend; ++p)
	if (*p == name) {
	    mypf(ERR_HANDLE, "%r attempt to write %s again\n", name);
	    twice = Yes;
	    break;	/* Don't report this twice	*/
	}

    /* Remember the name: */
    if (outputend >= outputfiles + MAX_GENER)
	fatal("Implementation limit: MAX_GENER\n");
    *outputend++ = name;	/* Add to list	*/

#ifndef GEMDOS	/* We cannot rename in an understandable way */
    if (!twice && (options & OPT_BACKUP)) {
	char to[1024];
	/*
	 *	We promised to backup files as <WhatEver>.BAK
	 *	The fastest check for availability is try-and-error
	 */
	sprintf(to, "%s.BAK", buf);	/* Append ".BAK" extension	*/
	errno = 0;
#ifdef SYSV
	if (link(buf, to) == 0 && unlink(buf) < 0)
	    unlink(to);
#else
	    rename(buf, to);
#endif
	/*
	 *	Errors: 0 -- no error: it worked.
	 *	ENOENT -- no error: it needn't.
	 */
	if (errno != 0 && errno != ENOENT) {
	    mypf(ERR_HANDLE, "Cannot move %s to %s: %s\n",
		 buf, to, error_string(errno));
	    fatal("Can't backup old file\n");
	}
    }
#endif
    h = GetFreeHandle();
    files[h].f_name = name;
    files[h].f_indent = 0;
    files[h].f_blks = files[h].f_last = NULL;
    return h;
} /* OpenFile */

/*
 *	Add a block to a handle. Don't initialise, since
 *	you don't know what the block will be used for.
 */
/* static */ struct block *
AddBlock(h)
    handle h;
{
    struct block *bp;
    chkhandle(h);
    assert((files[h].f_last == NULL) == (files[h].f_blks == NULL));
    bp = new(struct block);
    bp->bl_next = NULL;
    if (files[h].f_last != NULL) {
	files[h].f_last->bl_next = bp;	/* Link with previous last one	*/
    } else {				/* First block to create	*/
	files[h].f_blks = bp;
    }
    files[h].f_last = bp;		/* Link with handle		*/
    return bp;
} /* AddBlock */

/*
 *	Create a backpatch, return the handle.
 *	It is legal to make backpatches in backpatches,
 *	which ail doesn't do.
 */
handle
BackPatch(h)
    handle h;
{
    handle nh;	/* The fresh handle	*/
    struct block *bp;
    assert(files[h].f_name != NULL);
    if (options & OPT_NOOUTPUT) return 4;
    /* Create and init the new handle	*/
    nh = GetFreeHandle();
    files[nh].f_name = &is_patch;
    files[nh].f_indent = files[h].f_indent;
    files[nh].f_blks = files[nh].f_last = NULL;
    /* Link with original handle	*/
    bp = AddBlock(h);
    bp->bl_size = -1;	/* Identify as backpatch	*/
    bp->bl_patch = nh;
    return nh;
} /* BackPatch */

/*
 *	Increase the size of a handle's last block
 *	with the chunk size, or add a new block.
 *	Growing a block makes writing the files cheaper,
 *	since it decreases the number of write()'s.
 */
static struct block *
GrowHandle(h, at_least)
    handle h;
    unsigned int at_least;	/* How many chars to allocate at least?	*/
{
    struct block *bp;

    /* stackchk(); */
    /* Round up a bit:	*/
    if (at_least < LITTLE_CHUNK) at_least = LITTLE_CHUNK;
    else {	/* at_least is supposed to be small, CHUNK_SIZE is big	*/
	assert(at_least <= CHUNK_SIZE)
	at_least = CHUNK_SIZE;
    }
    if (files[h].f_last == NULL ||
      files[h].f_last->bl_size < 0 ||
      files[h].f_last->bl_size >= CHUNK_SIZE) {
	/* Get new block	*/
	bp = AddBlock(h);
	bp->bl_cnt = 0;
	bp->bl_text = MyAlloc(at_least);
	bp->bl_size = at_least;
    } else {
	/* Grow old block	*/
	bp = files[h].f_last;
	bp->bl_size = CHUNK_SIZE;
	/* Wow! This must make lint really happy!	*/
	bp->bl_text = (char *) realloc(bp->bl_text, (unsigned int) bp->bl_size);
	if (bp->bl_text == NULL) fatal("Can't realloc block\n");
    }
    return bp;
} /* GrowHandle */

/********************************************************\
 *							*
 *	The routines that actually put text		*
 *	in the blocks					*
 *							*
\********************************************************/

/*
 *	Append one char to a file
 *	The char must be declared as an int, since I've
 *	seen compilers that barf on taking the address
 *	of an argument of type char (this has to do with
 *	argument widening).
 */
void
AddChar(h, ch)
    register handle h;
    int ch;
{
    register struct block *bp;
    if (h < 0) {	/* No way to handle write errors here... */
#ifdef USE_STDIO
	putc(ch, (h == ERR_HANDLE) ? STD_ERR : STD_OUT);
#else
	char avoid_compiler_bugs;
	avoid_compiler_bugs = ch;
	(void) write(-h, &avoid_compiler_bugs, 1);
#endif
	return;
    }
    chkhandle(h);
    bp = files[h].f_last;		/* To which block?	*/
    assert (bp == NULL || bp->bl_size < 0 || bp->bl_cnt <= bp->bl_size);
    /* Can we use the last block?	*/
    if (bp == NULL || bp->bl_size < 0 || bp->bl_size <= bp->bl_cnt)
	bp = GrowHandle(h, 1);
    bp->bl_text[bp->bl_cnt++] = ch;	/* Add the character	*/
} /* AddChar */

/*
 *	Add several chars
 */
void
AddBuf(h, buf, n)
    handle h;
    register char *buf;
    register int n;
{
    /* if (stackchk() < 50) abort(); */
    assert(n >= 0);
    if (h < 0) {
	assert(h >= -2);
	(void) WRITE((h == ERR_HANDLE) ? STD_ERR : STD_OUT, buf, n);
    } else {
	struct block *tail;
	int pre;	/* How many chars we still can put in the block	*/
#if 0
	assert(h < MAX_OPEN);
#else
	if (h >= MAX_OPEN) abort();
#endif
	tail = files[h].f_last;
	pre = (tail == NULL || tail->bl_size < 0) ? 0 :
	    tail->bl_size - tail->bl_cnt;
	if (pre > 0) {		/* print in current last block	*/
	    if (pre > n) pre = n;
	    (void) memcpy(tail->bl_text + tail->bl_cnt, buf, pre);
	    tail->bl_cnt += pre;
	    n -= pre; buf += pre;
	    assert(n >= 0);
	    if (n >= CHUNK_SIZE) fatal("Implementation error in AddBuf");
	}
	if (n != 0) {		/* Print the residu	*/
	    tail = GrowHandle(h, (unsigned) n);
	    (void) memcpy(tail->bl_text + tail->bl_cnt, buf, n);
	    tail->bl_cnt += n;
	}
    }
} /* AddBuf */

/*
 *	Insert a newline and the right amount of tabs and spaces.
 *	The f_indent field holds the indent level. One indent level
 *	is equivalent with four space, two levels is equivalent
 *	with one tab.
 */
void
Indent(h)
    handle h;
{
    int t, odd;
    AddChar(h, '\n');
    switch (h) {
    case ERR_HANDLE:
	t = err_indent;
	break;
    case OUT_HANDLE:
	t = out_indent;
	break;
    default:
	t = files[h].f_indent;
    }
    odd = (t & 0x01); t /= 2;
    while (t--) AddChar(h, '\t');
    if (odd) AddBuf(h, "    ", 4);
} /* Indent */
