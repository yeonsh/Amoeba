/*	@(#)dvi_init.c	1.1	91/11/21 13:31:26 */
/*
 * DVI previewer for X.
 *
 * Eric Cooper, CMU, September 1985.
 *
 * Code derived from dvi-imagen.c.
 *
 * Modification history:
 * 1/1986	Modified for X.10 by Bob Scheifler, MIT LCS.
 * 7/1988	Modified for X.11 by Mark Eichin, MIT
 * 12/1988	Added 'R' option, toolkit, magnifying glass
 *			--Paul Vojta, UC Berkeley.
 * 2/1989	Added tpic support	--Jeffrey Lee, U of Toronto
 * 4/1989	Modified for System V by Donald Richardson, Clarkson Univ.
 * 3/1990	Added VMS support	--Scott Allendorf, U of Iowa
 *
 *	Compilation options:
 *	SYSV	compile for System V
 *	VMS	compile for VMS
 *	X10	compile for X10
 *	NOTOOL	compile without toolkit (X11 only)
 *	BUTTONS	compile with buttons on the side of the window (needs toolkit)
 *	MSBITFIRST	store bitmaps internally with most significant bit first
 *	BMSHORT	store bitmaps in shorts instead of bytes
 *	BMLONG	store bitmaps in longs instead of bytes
 *	ALTFONT	default for -altfont option
 *	A4	use European size paper
 */

#include <stdio.h>
#include <ctype.h>
#include "xdvi.h"
#include "dvi.h"
#include <sys/stat.h>

#define	dvi_oops(str)	longjmp(dvi_env, (int) str);
#define XtOffset(type, field)    ((unsigned int)&(((type)NULL)->field))

static	struct stat fstatbuf;		/* mechanism to see if file was */
time_t	dvi_time;			/* modified since last usage */

struct font *current_font = NULL;	/* ptr into linked list of fonts */
ubyte	maxchar;

static	Boolean	font_not_found;
static	struct font **old_fonts;	/* used by read_postamble */

int	n_fonts_left	= 32767;	/* for LRU management of fonts */

char	*realloc();

/*
 * DVI preamble and postamble information.
 */
int	current_page;
Boolean	spec_warn;
int	total_pages;
double	fraction;
int	maxstack;
static	char	job_id[300];
static	long	numerator, denominator, magnification;

/*
 * Table of page offsets in DVI file, indexed by page number - 1.
 * Initialized in prepare_pages().
 */
long	*page_offset;

/*
 * Offset in DVI file of last page, set in read_postamble().
 */
static	long	last_page_offset;

#if defined(sun) && !defined(AMOEBA)
char	*sprintf();
#endif

char	*malloc();
FILE	*pxl_open();

/*
 *      define_font reads the rest of the fntdef command and then reads in
 *      the specified pixel file, adding it to the global linked-list holding
 *      all of the fonts used in the job.
 */
static	void
define_font(cmnd)
	ubyte cmnd;
{
        register struct font *fontp;
	struct font **fontpp = old_fonts;
	struct font *fontp1;
	int len;
	int design;
	int size;

	fontp = (struct font *) malloc((unsigned) sizeof(struct font));
	if (fontp == NULL)
		oops("Can't allocate memory for font");
	fontp->TeXnumber = num(dvi_file, (ubyte) cmnd - FNTDEF1 + 1);
	(void) four(dvi_file);	/* checksum */
	fontp->scale = four(dvi_file);
	design = four(dvi_file);
	len = one(dvi_file) + one(dvi_file);
	fontp->fontname = malloc((unsigned) len + 1);
	Fread(fontp->fontname, sizeof(char), len, dvi_file);
	fontp->fontname[len] = '\0';
	if(debug & DBG_PK)
	  Printf("Define font \"%s\" scale=%d design=%d\n",
	    fontp->fontname, fontp->scale, design);
	fontp->size = size = ((double) fontp->scale / design * magnification
	    * pixels_per_inch * 0.005) + 0.5;
	fontp->scale = fontp->scale * fraction;
	/*
	 * reuse font if possible
	 */
	for (;;) {
	    fontp1 = *fontpp;
	    if (fontp1 == NULL) {
		read_font_index_proc read_font_index;
		char	*font_found;
		int	size_found;
		int	dpi = (size + 2) / 5;

		if (n_fonts_left == 0)
		    close_a_file();
		fontp->file = pxl_open(fontp->fontname, &font_found,
		    size, &size_found, &fontp->factor, &fontp->filename,
		    &read_font_index);
		if (fontp->file == NULL) {
		    Fprintf(stderr, "Can't find font %s.\n", fontp->fontname);
		    font_not_found = True;
		    return;
		}
		--n_fonts_left;
		if (font_found != NULL) {
		    Fprintf(stderr,
			"Can't find font %s; using %s instead at %d dpi\n",
			fontp->fontname, font_found, dpi);
		    free(fontp->fontname);
		    fontp->fontname = font_found;
		}
		else if (25 * size_found > 26 * size ||
			25 * size > 26 * size_found)
		    Fprintf(stderr,
			"Can't find font %s at %d dpi; using %d dpi instead.\n",
			fontp->fontname, dpi, (size_found + 2) / 5);
		fontp->factor = fontp->factor * size / size_found;
		maxchar = 255;
		(*read_font_index)(fontp);
		while (maxchar > 0 && fontp->glyph[maxchar].addr == 0)
		    --maxchar;
		if (maxchar < 255)
		    fontp = (struct font *) realloc((char *) fontp,
			XtOffset(struct font *, glyph[(int) maxchar + 1]));
		fontp->maxchar = maxchar;
		break;
	    }
	    if (strcmp(fontp->fontname, fontp1->fontname) == 0
		    && size == fontp1->size) {
		*fontpp = fontp1->next;
		free(fontp->fontname);
		free((char *) fontp);
		fontp = fontp1;
		if (list_fonts)
		    fputs("(reusing) ",stdout);
		break;
	    }
	    fontpp = &fontp1->next;
	}

	if (old_fonts == &current_font) old_fonts = &fontp->next;
	fontp->next = current_font;
	current_font = fontp;
	if (list_fonts)
	    puts(fontp->fontname);
}

/*
 *      process_preamble reads the information in the preamble and stores
 *      it into global variables for later use.
 */
static
process_preamble()
{
        ubyte   k;

        if (one(dvi_file) != PRE)
		dvi_oops("DVI file doesn't start with preamble");
	if (one(dvi_file) != 2)
		dvi_oops("Wrong version of DVI output for this program");
	numerator     = four(dvi_file);
	denominator   = four(dvi_file);
	magnification = four(dvi_file);
	fraction = (((double) numerator * magnification)
	                                 / ((double) denominator * 1000.));
	fraction = fraction * (((long) pixels_per_inch)<<16) / 254000;
	k = one(dvi_file);
	Fread(job_id, sizeof(char), (int) k, dvi_file);
	job_id[k] = '\0';
}

/*
 *      find_postamble locates the beginning of the postamble
 *	and leaves the file ready to start reading at that location.
 */
static
find_postamble()
{
	ubyte byte;

	Fseek(dvi_file, (long) -4, 2);
	while (four(dvi_file) !=
		((long) TRAILER << 24 | TRAILER << 16 | TRAILER << 8 | TRAILER))
	    Fseek(dvi_file, (long) -5, 1);
	Fseek(dvi_file, (long) -5, 1);
	for (;;) {
		byte = one(dvi_file);
		if (byte != TRAILER) break;
		Fseek(dvi_file, (long) -2, 1);
	}
	if (byte != 2)
		dvi_oops("Wrong version of DVI output for this program");
	Fseek(dvi_file, (long) -5, 1);
	Fseek(dvi_file, sfour(dvi_file), 0);
}

/*
 *      read_postamble reads the information in the postamble,
 *	storing it into global variables.
 *      It also takes care of reading in all of the pixel files for the fonts
 *      used in the job.
 */
static
read_postamble()
{
        ubyte   cmnd;

        if (one(dvi_file) != POST)
	    dvi_oops("Postamble doesn't begin with POST");
	last_page_offset = four(dvi_file);
	if (numerator != four(dvi_file)
	          ||  denominator != four(dvi_file)
		  ||  magnification != four(dvi_file))
	    dvi_oops("Postamble doesn't match preamble");
		/* read largest box height and width */
	unshrunk_page_h = (spellfour(dvi_file) >> 16) + pixels_per_inch;
	if (unshrunk_page_h < unshrunk_paper_h)
	    unshrunk_page_h = unshrunk_paper_h;
	unshrunk_page_w = (spellfour(dvi_file) >> 16) + pixels_per_inch;
	if (unshrunk_page_w < unshrunk_paper_w)
	    unshrunk_page_w = unshrunk_paper_w;
	maxstack = two(dvi_file);
	total_pages = two(dvi_file);
	old_fonts = &current_font;
	font_not_found = False;
	do {
	    switch(cmnd = one(dvi_file)) {
	        case FNTDEF1:
	        case FNTDEF2:
	        case FNTDEF3:
	        case FNTDEF4:
		    define_font(cmnd);
		    break;
		case POSTPOST:
		    break;
		default:
		    dvi_oops("Non-fntdef command found in postamble");
	    }
	} while (cmnd != POSTPOST);
	if (font_not_found)
	    dvi_oops("Not all pixel files were found");
	/*
	 * free up fonts no longer in use
	 */
	{
	    struct font *fontp = *old_fonts;
	    struct font *fontp1;
	    register struct glyph *g;
	    *old_fonts = NULL;
	    while (fontp != NULL) {
		if (fontp->file != NULL) {
		    Fclose(fontp->file);
		    ++n_fonts_left;
		}
		free(fontp->fontname);
		free(fontp->filename);
		for (g = fontp->glyph; g <= fontp->glyph + fontp->maxchar; ++g)
		{
		    if (g->bitmap.bits) free(g->bitmap.bits);
		    if (g->bitmap2.bits) free(g->bitmap2.bits);
		}
		fontp1 = fontp->next;
		free((char *) fontp);
		fontp = fontp1;
	    }
	}
}

static
prepare_pages()
{
	int i;

        stack = (struct frame *)
		malloc((unsigned) sizeof(struct frame) * (maxstack+1));
        if (stack == NULL)
		oops("Can't allocate stack space (%d frames)", maxstack);
	page_offset = (long *) malloc((unsigned) total_pages * sizeof(long));
        if (page_offset == NULL)
		oops("Can't allocate page directory (%d pages)",
			total_pages);
	i = total_pages;
	page_offset[--i] = last_page_offset;
	Fseek(dvi_file, last_page_offset, 0);
	/*
	 * Follow back pointers through pages in the DVI file,
	 * storing the offsets in the page_offset table.
	 */
	while (i > 0) {
		Fseek(dvi_file, (long) (1+4+(9*4)), 1);
		Fseek(dvi_file, page_offset[--i] = four(dvi_file), 0);
	}
}

init_page()
{
	page_w = ROUNDUP(unshrunk_page_w, mane.shrinkfactor) + 2;
	page_h = ROUNDUP(unshrunk_page_h, mane.shrinkfactor) + 2;
}

/*
 *	init_dvi_file is the main subroutine for reading the startup information
 *	from the dvi file.
 */
static
init_dvi_file()
{
	(void) fstat(fileno(dvi_file), &fstatbuf);
	dvi_time = fstatbuf.st_mtime;
	process_preamble();
	find_postamble();
	read_postamble();
	prepare_pages();
	init_page();
	if (current_page >= total_pages) current_page = total_pages - 1;
	spec_warn = True;
}

/**
 **	open_dvi_file opens the dvi file and calls init_dvi_file() to
 **	initialize it.
 **/

open_dvi_file()
{
	char *errmsg;

	if ((dvi_file = fopen(dvi_name, OPEN_MODE)) == NULL) {
		int n = strlen(dvi_name);
		char *file = dvi_name;

		if (strcmp(dvi_name + n - sizeof(".dvi") + 1, ".dvi") == 0) {
			perror(dvi_name);
			exit(1);
		}
		dvi_name = malloc((unsigned) n + sizeof(".dvi"));
		Sprintf(dvi_name, "%s.dvi", file);
		if ((dvi_file = fopen(dvi_name, OPEN_MODE)) == NULL) {
			perror(dvi_name);
			exit(1);
		}
	}

	if (errmsg = (char *) setjmp(dvi_env)) oops(errmsg);
	init_dvi_file();
}

/**
 **	Release all shrunken bitmaps for all fonts.
 **/

reset_fonts()
{
        register struct font *f;
	register struct glyph *g;

	for (f = current_font; f != NULL; f = f->next)
	    for (g = f->glyph; g <= f->glyph + f->maxchar; ++g)
		if (g->bitmap2.bits) {
		    free(g->bitmap2.bits);
		    g->bitmap2.bits = NULL;
		}
}

/**
 **	Check for changes in dvi file.
 **/

Boolean
check_dvi_file()
{
	if (dvi_file == NULL || fstat(fileno(dvi_file), &fstatbuf) != 0
	    || fstatbuf.st_mtime != dvi_time) {
		if (dvi_file) Fclose(dvi_file);
		free((char *) stack);
		free((char *) page_offset);
		dvi_file = fopen(dvi_name, OPEN_MODE);
		if (dvi_file == NULL)
		    dvi_oops("Cannot reopen dvi file.");
		if (list_fonts) putchar('\n');
		init_dvi_file();
		redraw_page();
		return False;
	}
	return True;
}
