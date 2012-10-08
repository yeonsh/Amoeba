/*	@(#)pxl_open.c	1.1	91/11/21 13:31:36 */
/*
 *	pxl_open.c(font, font_ret, mag, mag_ret, factor, name, read_font_index)
 *	Find and open gf, pk, or pxl files in the given path, having the given
 *	name and magnification.  It tries gf files first, followed by pk and pxl
 *	files.  The path variable should be of the form path1:path2:...:pathn,
 *	and each of the paths will be tried successively.  Strings in pathi of
 *	the form %f, %p, and %d will be replaced by the font name, "gf" or "pk"
 *	or "pxl", and the magnification, respectively.  If no %f appears in a
 *	path specifier, then the string "/%f.%d%p" is added on the end.  This
 *	procedure is repeated for each of the possible magnifications allowed,
 *	and if it fails then the procedure will try changing the point size
 *	as well.  If all of the above fails, then alt_font will be tried.
 *
 *	If the file is found, then a file pointer is returned, and the following
 *	values are set:
 *		*font_ret  a pointer to a string containing the font name (if
 *			different from the font requested).
 *		*mag_ret  the actual magnification found.
 *		*factor	the ratio of the point sizes of the requested font
 *			and the font actually found (or 1.0 if altfont is used).
 *		*name	a pointer to a string containing the file name
 *		*read_font_index  a pointer to the read_index procedure to be
 *			used for the given font format.
 *
 *	If the file is not found, then the return value is NULL.
 *
 *	Often there are so many fonts that we need to manage the number of
 *	simultaneously open files.  In that case, the variable n_fonts_left
 *	gives the number of open files that are left, (initially MAXINT, set
 *	dynamically) and when it is necessary to close a file, these routines
 *	call close_a_file() which should free up a file descriptor.
 *
 */

#include <stdio.h>

#ifndef X10
#include <X11/Xos.h>	/* same as below */
#else	/* X10 */
#ifdef	SYSV
#include <string.h>
#define index strchr
#define rindex strrchr
#else /* SYSV */
#include <strings.h>
#endif /* SYSV */
#endif	/* X10 */

#include <errno.h>
extern	int	errno;

/*
 *	If you think you have to change DEFAULT_TAIL, then you haven't read the
 *	documentation closely enough.
 */
#ifndef	VMS
#define	PATH_SEP	':'
#define	DEFAULT_TAIL	"/%f.%d%p"
#else	/* VMS */
#include <string.h>
#define	index	strchr
#define	rindex	strrchr
#define	PATH_SEP	'/'
#define	DEFAULT_TAIL	":%f.%d%p"
#endif	/* VMS */

#ifndef	OPEN_MODE
#ifndef	VMS
#define	OPEN_MODE	"r"
#else
#define	OPEN_MODE	"r", "ctx=stm"
#endif
#endif	/* OPEN_MODE */

extern	int	n_fonts_left;
extern	char	*alt_font;

static	char	*font_path;
static	char	default_font_path[]	= DEFAULT_FONT_PATH;
static	int	*sizes, *sizend;
static	char	default_size_list[]	= DEFAULT_FONT_SIZES;

	/* the corresponding read_char procedures are handled in xdvi.h */
typedef	void (*read_font_index_proc)();
	/* struct font *fontp; */

read_font_index_proc read_GF_index, read_PK_index, read_PXL_index;

#if defined(sun) && !defined(AMOEBA)
char	*sprintf();
#endif

char	*malloc(), *getenv();
double	atof();

#define	Strcpy	(void) strcpy
#define	Sprintf	(void) sprintf

static	void
get_sizes(size_list, spp)
	char	*size_list;
	int	**spp;
{
	if (*size_list == PATH_SEP) ++size_list;
	for (;;) {
	    *(*spp)++ = atof(size_list) * 5 + 0.5;
	    size_list = index(size_list, PATH_SEP);
	    if (size_list == NULL) return;
	    ++size_list;
	}
}

init_pxl_open()
{
	char	*size_list;
	int	*sp, *sp1, *sp2;
	unsigned int n;
	char	*p;

	if ((font_path = getenv("XDVIFONTS")) == NULL)
	    font_path = default_font_path;
	else if (*font_path == PATH_SEP)
		/*concatenate default_font_path before font_path */
	    font_path = strcat(strcpy(malloc((unsigned)
		strlen(default_font_path) + strlen(font_path) + 1),
		default_font_path), font_path);

	size_list = getenv("XDVISIZES");
	n = 1;	/* count number of sizes */
	if (size_list == NULL || *size_list == PATH_SEP)
	    for (p = default_size_list; (p = index(p, PATH_SEP)) != NULL; ++p)
		++n;
	if (size_list != NULL)
	    for (p = size_list; (p = index(p, PATH_SEP)) != NULL; ++p) ++n;
	sizes = (int *) malloc(n * sizeof(int));
	sizend = sizes + n;
	sp = sizes;	/* get the actual sizes */
	if (size_list == NULL || *size_list == PATH_SEP)
	    get_sizes(default_size_list, &sp);
	if (size_list != NULL) get_sizes(size_list, &sp);

	/* bubble sort the sizes */
	sp1 = sizend - 1;	/* extent of this pass */
	do {
	    sp2 = NULL;
	    for (sp = sizes; sp < sp1; ++sp)
		if (*sp > sp[1]) {
		    int i = *sp;
		    *sp = sp[1];
		    sp[1] = i;
		    sp2 = sp;
		}
	}
	while ((sp1 = sp2) != NULL);
}

static	FILE *
formatted_open(path, font, pxl, mag, name)
	char	*path, *font, *pxl;
	int	mag;
	char	**name;
{
	char	*p = path,
		nm[128],
		*n = nm,
		c;
	int	f_used = 0;
	FILE	*f;

	for (;;) {
	    c = *p++;
	    if (c==PATH_SEP || c=='\0') {
		if (f_used) break;
		p = DEFAULT_TAIL;
		continue;
	    }
	    if (c=='%') {
		c = *p++;
		switch (c) {
		    case 'f':
			f_used=1;
			Strcpy(n, font);
			break;
		    case 'p':
			Strcpy(n, pxl);
			break;
		    case 'd':
			Sprintf(n, "%d", mag);
			break;
		    default:
			*n++ = c;
			*n = '\0';
		}
		n += strlen(n);
	    }
	    else *n++ = c;
	}
	*n = '\0';
	f = fopen(nm, OPEN_MODE);
	if (f == NULL && errno == EMFILE) {
	    n_fonts_left = 0;
	    close_a_file();
	    f = fopen(nm, OPEN_MODE);
	}
	if (f != NULL) {
	    *name = malloc((unsigned) (n - nm + 1));
	    Strcpy(*name, nm);
	}
	return f;
}

static	FILE *
pre_pxl_open(font, mag, mag_ret, name, read_font_index)
	char	*font;
	int	mag, *mag_ret;
	char	**name;
	read_font_index_proc *read_font_index;
{
	char	*p;
	FILE	*f;
	int	*p1, *p2, pxlmag, pkmag;

	/*
	 * Loop over sizes.  Try closest sizes first.
	 */
	for (p2 = sizes; p2 < sizend; ++p2) if (*p2 >= mag) break;
	p1 = p2;
	for (;;) {
	    if (p1 <= sizes)
		if (p2 >= sizend) return NULL;
		else pxlmag = *p2++;
	    else if (p2 >= sizend || mag * mag <= p1[-1] * *p2) pxlmag = *--p1;
		else pxlmag = *p2++;
	    *mag_ret = pxlmag;
	    pkmag = (pxlmag + 2) / 5;
	    /*
	     * loop over paths
	     */
	    for (p = font_path;;) {
		if (read_GF_index &&
		    (f = formatted_open(p, font, "gf", pkmag, name)) != NULL) {
			*read_font_index = read_GF_index;
			return f;
		}
		if (read_PK_index &&
		    (f = formatted_open(p, font, "pk", pkmag, name)) != NULL) {
			*read_font_index = read_PK_index;
			return f;
		}
		if (read_PXL_index &&
		    (f = formatted_open(p, font, "pxl", pxlmag, name)) != NULL)
		{
			*read_font_index = read_PXL_index;
			return f;
		}
		p = index(p, PATH_SEP);
		if (p == NULL) break;
		++p;
	    }
	}
}

FILE *
pxl_open(font, font_ret, mag, mag_ret, factor, name, read_font_index)
	char	*font, **font_ret;
	int	mag, *mag_ret;
	float	*factor;
	char	**name;
	read_font_index_proc *read_font_index;
{
	FILE	*f;
	int	actual_pt, low_pt, high_pt, trial_pt;
	char	fn[50], *fnend;

	*factor = 1.0;
	f = pre_pxl_open(font, mag, mag_ret, name, read_font_index);
	if (f != NULL) {
	    *font_ret = NULL;
	    return f;
	}
	Strcpy(fn, font);
	fnend = fn + strlen(fn);
	while (fnend > fn && fnend[-1] >= '0' && fnend[-1] <= '9') --fnend;
	actual_pt = low_pt = high_pt = atoi(fnend);
	if (actual_pt) {
	    low_pt = actual_pt - 1;
	    high_pt = actual_pt + 1;
	    for (;;) {
		if (2 * low_pt >= actual_pt &&
		    (low_pt * high_pt > actual_pt * actual_pt ||
		    high_pt > actual_pt + 5))
			trial_pt = low_pt--;
		else if (high_pt > actual_pt + 5) break;
		else trial_pt = high_pt++;
		Sprintf(fnend, "%d", trial_pt);
		f = pre_pxl_open(fn, mag * actual_pt / trial_pt, mag_ret, name,
		    read_font_index);
		if (f != NULL) {
		    *font_ret = strcpy(malloc((unsigned) strlen(fn) + 1), fn);
		    *factor = (float) actual_pt / trial_pt;
		    return f;
		}
	    }
	}
	if (alt_font != NULL) {
	    *font_ret = alt_font;
	    f = pre_pxl_open(alt_font, mag, mag_ret, name, read_font_index);
	    if (f != NULL)
		*font_ret = strcpy(malloc((unsigned) strlen(alt_font) + 1),
		    alt_font);
	}
	return f;
}
