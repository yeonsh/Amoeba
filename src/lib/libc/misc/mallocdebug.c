/*	@(#)mallocdebug.c	1.1	96/02/27 11:11:44 */
#undef __MAL_DEBUG
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* #define CHECK_ALL_CHUNCKS */
/* #define FULLDEBUG */

typedef struct mdb_header mdb_h_t, *mdb_h_p;
typedef struct mdb_trailer mdb_t_t, *mdb_t_p;

/* note: this structure is a multiple of 8 bytes big */
struct mdb_header {
    size_t   mdbh_size;		/* client size allocated, excluding alignment*/
    long     mdbh_magic1;
    char    *mdbh_file;
    long     mdbh_line;
    long     mdbh_magic2;
    long     mdbh_magic3;	/* Spillage */
};
#define HeaderSize	(sizeof(struct mdb_header))

struct mdb_trailer {
    long     mdbt_magic5;
    char    *mdbt_file;
    long     mdbt_line;
    long     mdbt_magic6;
};
#define TrailerSize	(sizeof(struct mdb_trailer))

#define MAGIC1	0xFEEBBEEF
#define MAGIC2	0xDEADDEAD
#define MAGIC3	0x12345678
#define MAGIC4	0xDADADADA
#define MAGIC5	0x87654321
#define MAGIC6	0xF0F0F0F0

#define MAGICB	'?'	/* magic byte to see if alignment bytes are untouched*/
#define FREED	0x13579BDF

/* Round up 'n' to a multiple of 'a', where 'a' is a power of 2 */
#define	ALIGN(n, a)	(((n) + (a) - 1) & ~((a) - 1))

#ifdef KERNEL
#define message printf
#else
/*VARARGS1*/
#ifdef __STDC__
#include <stdarg.h>
static void message(const char *format, ...)
#else
#include <varargs.h>
static void message(format, va_alist) char *format; va_dcl
#endif
{
    static char buf[1000];
    va_list ap;
    int size;

#ifdef __STDC__
    va_start(ap, format);
#else
    va_start(ap);
#endif
    size = vsprintf(buf, format, ap);
    va_end(ap);

    write(2, buf, size);
}
#endif

static char *
lastcomp(path)
char *path;
{
    char *slash;

    if ((slash = strrchr(path, '/')) != NULL) {
	return slash + 1;
    } else {
	return path;
    }
}

void
mal_init(hp, tp, size, file, line)
mdb_h_p hp;
mdb_t_p tp;
size_t size;	/* size of block requested */
char *file;
int line;
{
    char *ptr = (char *)hp + HeaderSize;
    register char *p;

    hp->mdbh_size = size;

    hp->mdbh_magic1 = MAGIC1;
    hp->mdbh_file = file;
    hp->mdbh_line = line;
    hp->mdbh_magic2 = MAGIC2;
    hp->mdbh_magic3 = MAGIC3;

    tp->mdbt_magic5 = MAGIC5;
    tp->mdbt_file = file;
    tp->mdbt_line = line;
    tp->mdbt_magic6 = MAGIC6;

    /* set the extra (alignment) bytes allocated to MAGICB */
    for (p = ptr + size; p < (char *)tp; p++) {
	*p = MAGICB;
    }
}

static int stats_malloc = 0;
static int stats_realloc = 0;
static int stats_free = 0;
static int stats_calloc = 0;

static void
failure(operation)
char *operation;
{
    message("failure in operation `%s'\n", operation);
    message("statistics:\n");
    message("malloc : %d\n", stats_malloc);
    message("calloc : %d\n", stats_calloc);
    message("realloc: %d\n", stats_realloc);
    message("free   : %d\n", stats_free);
#ifdef KERNEL
    panic("malloc administration corrupted");
#else
    abort();
#endif
}

static int
safe_to_print(file)
char *file;
{
#ifndef KERNEL
    /* avoid locking myself up in low level routines calling this module */
    return (strstr(file, "sysam") == NULL);
#else
    return 1;
#endif
}

static void
dumpbytes(start, end)
long *start;
long *end;
{
    message("0x%lx: ", start);
    for (; start <= end; start++) {
	message("0x%lx ", *start);
    }
    message("\n");
}

static int
mal_check(ptr)
_VOIDSTAR ptr;
{
    register char *p;
    mdb_h_p hp;
    mdb_t_p tp;

    hp = (mdb_h_p)((char *)ptr - HeaderSize);

    if (hp->mdbh_magic1 != MAGIC1 || hp->mdbh_magic2 != MAGIC2) {
	/* maybe it already has been free()d */
	if (hp->mdbh_magic1 == FREED && hp->mdbh_magic2 == FREED) {
	    message("malloced memory already freed in file %s, line %d\n",
		   hp->mdbh_file, hp->mdbh_line);
	} else {
	    message("essential malloced memory overwritten\n");
	    dumpbytes(&hp->mdbh_magic1, &hp->mdbh_magic3 + 4);
	}
	return 0;
    }

    if (hp->mdbh_magic3 != MAGIC3) {
	message("memory before 0x%lx, size %d, from %s, line %d corrupted\n",
		ptr, hp->mdbh_size, hp->mdbh_file, hp->mdbh_line);
	dumpbytes(&hp->mdbh_magic1, &hp->mdbh_magic3 + 4);
	return 0;
    }

    tp = (mdb_t_p)((char *)hp + HeaderSize + ALIGN(hp->mdbh_size, 4));
    if (tp->mdbt_magic5 != MAGIC5 || tp->mdbt_magic6 != MAGIC6)
    {
	message("memory after 0x%lx, size %d, from %s, line %d corrupted\n",
		ptr, hp->mdbh_size, hp->mdbh_file, hp->mdbh_line);

	dumpbytes(&tp->mdbt_magic5 - 16, &tp->mdbt_magic6);

	return 0;
    }

    /* check alignment bytes */
    for (p = (char *)ptr + hp->mdbh_size; p < (char *)tp; p++) {
	if (*p != MAGICB) {
	    message("align byte overwritten (%x instead of %c): %s, line %d\n",
		   *p, MAGICB, hp->mdbh_file, hp->mdbh_line);
	    return 0;
	}
    }

    /* passed all checks */
    return 1;
}


#ifdef CHECK_ALL_CHUNCKS

#define MAX_MALCHUNCKS	10000 /* arbitrary upperbound */
static char *Mal_chuncks[MAX_MALCHUNCKS];
static int Nchuncks;

static void
mal_install(ptr)
char *ptr;
{
    int i;

    for (i = 0; i < MAX_MALCHUNCKS; i++) {
	if (Mal_chuncks[i] == NULL) {
	    Mal_chuncks[i] = ptr;
	    Nchuncks++;
	    return;
	}
    }

    message("mal_debug: cannot install chunck at 0x%lx\n", ptr);
}

static void
mal_deinstall(ptr)
char *ptr;
{
    int i;

    for (i = 0; i < MAX_MALCHUNCKS; i++) {
	if (Mal_chuncks[i] == ptr) {
	    Mal_chuncks[i] = NULL;
	    Nchuncks--;
	    return;
	}
    }

    message("mal_debug: cannot deinstall chunck at 0x%lx\n", ptr);
}

int
mal_check_all()
{
    int errors;
    int i, found;

    errors = 0;
    found = 0;
    for (i = 0; i < MAX_MALCHUNCKS && found < Nchuncks; i++) {
	if (Mal_chuncks[i] != NULL) {
	    found++;
	    if (!mal_check(Mal_chuncks[i])) {
		errors++;
	    }
	}
    }

    return errors;
}

#endif /* CHECK_ALL_CHUNCKS */


_VOIDSTAR
_malloc_debug(size, file, line)
size_t   size;
char 	*file;
int      line;
{
    char *p, *ret_ptr;
    mdb_h_p hp;
    mdb_t_p tp;
    size_t allocsize;

    stats_malloc++;

    /* malloc_debug(0) must return NULL, just like malloc(0)! */
    if (size == 0) {
	return NULL;
    }

    allocsize = HeaderSize + ALIGN(size, 4) + TrailerSize;

    p = (char *)_malloc_real(allocsize);

    if (p == NULL) {
	return NULL;
    }

    ret_ptr = p + HeaderSize;
    hp = (mdb_h_p) p;
    tp = (mdb_t_p)(p + HeaderSize + ALIGN(size, 4));

    mal_init(hp, tp, size, file, line);

#ifdef CHECK_ALL_CHUNCKS
    mal_install(ret_ptr);
    (void) mal_check_all();
#endif
#ifdef FULLDEBUG
    if (safe_to_print(file)) {
	message("M(%ld,%s,%d)=0x%lx\n",	size, lastcomp(file), line, ret_ptr);
    }
#endif

    return (_VOIDSTAR)ret_ptr;
}

void
_free_debug(ptr, file, line)
_VOIDSTAR ptr;
char *file;
int line;
{
    mdb_h_p hp;
    size_t size;

#ifdef FULLDEBUG
    if (safe_to_print(file)) {
	message("F(0x%lx,%s)\n", ptr, lastcomp(file));
    }
#endif

    if (ptr == NULL) {	/* act like free(NULL) */
	return;
    }

    hp = (mdb_h_p)((char *)ptr - HeaderSize);
    size = hp->mdbh_size;

    stats_free++;

    if (!mal_check(ptr)) {
	message("file %s, line %d: failed to free memory at %lx, size %d\n",
		file, line, ptr, size);
	failure("free");
    }

#ifdef CHECK_ALL_CHUNCKS
    mal_deinstall(ptr);
    (void) mal_check_all();
#endif

    /* invalidate memory and trailer */
    memset(ptr, MAGICB, ALIGN(size, 4) + TrailerSize);
    *((char *)ptr + size - 1) = 0;	/* terminate (maybe it's a string) */

    /* mark header in a special way */
    hp->mdbh_magic1 = FREED;
    hp->mdbh_magic2 = FREED;
    hp->mdbh_file = file;
    hp->mdbh_line = line;

    _free_real((_VOIDSTAR)hp);
}

_VOIDSTAR
_realloc_debug(ptr, size, file, line)
_VOIDSTAR ptr;
char   *file;
size_t  size;
int     line;
{
    char *p, *ret_ptr;
    mdb_h_p hp;
    mdb_t_p tp;
    size_t allocsize;

#ifdef FULLDEBUG
    if (safe_to_print(file)) {
	message("R(0x%lx,%ld,%s,%d)\n", ptr, size, lastcomp(file), line);
    }
#endif

    if (ptr == NULL) {		/* realloc(NULL, size) must do malloc(size) */
	return _malloc_debug(size, file, line);
    } else if (size == 0) {	/* realloc(ptr, 0) must do free(ptr) */
	_free_debug(ptr, file, line);
	return NULL;
    }

    if (!mal_check(ptr)) {
	message("file %s, line %d: failed to realloc memory at %lx, size %d\n",
		file, line, ptr, size);
	failure("realloc");
    }
#ifdef CHECK_ALL_CHUNCKS
    mal_deinstall(ptr);
#endif

    allocsize = HeaderSize + ALIGN(size, 4) + TrailerSize;

    stats_realloc++;

    p = (char *)_realloc_real((_VOIDSTAR)((char *)ptr - HeaderSize),
			      allocsize);

    if (p == NULL) {
	return NULL;
    }

    ret_ptr = p + HeaderSize;
    hp = (mdb_h_p) p;
    tp = (mdb_t_p)(p + HeaderSize + ALIGN(size, 4));
    mal_init(hp, tp, size, file, line);

#ifdef CHECK_ALL_CHUNCKS
    mal_install(ptr);
    (void) mal_check_all();
#endif

    return (_VOIDSTAR)ret_ptr;
}

_VOIDSTAR
_calloc_debug(num, elmsize, file, line)
size_t num;
size_t elmsize;
char *file;
int line;
{
    register _VOIDSTAR  p;
    register size_t     size;

#ifdef FULLDEBUG
    if (safe_to_print(file)) {
	message("C");
    }
#endif
    size = num * elmsize;
    if ((p = _malloc_debug(size, file, line)) != NULL) {
        memset(p, 0, size);
    }

    stats_calloc++;

    return p;
}
