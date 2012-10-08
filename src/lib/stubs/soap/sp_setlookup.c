/*	@(#)sp_setlookup.c	1.4	94/04/07 11:10:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "sp_stubs.h"
#include "sp_buf.h"

#define MAX_SETLOOKUP 100	/* avoid overflowing the output buffer */

/* To allow one copy of the code to cleanup and return: */
#define Failure(val)		{ err = (val); goto failure; }

static errstat
sp_do_setlookup(n, n_done, in, out)
int       n;
int      *n_done;
sp_entry  in[];
sp_result out[];
{
    union sp_buf *inbuf = NULL, *outbuf = NULL;
    header    hdr;
    int	      i, done;
    char     *p, *e;
    char     *save_p;
    errstat   err;

    if ((inbuf = sp_getbuf()) == NULL) {
	Failure(STD_NOSPACE);
    }
    if ((outbuf = sp_getbuf()) == NULL) {
	Failure(STD_NOSPACE);
    }

    p = inbuf->sp_buffer;
    e = &inbuf->sp_buffer[sizeof(inbuf->sp_buffer)];

    save_p = NULL;
    done = 0;
    for (i = 0; i < n && i < MAX_SETLOOKUP; i++) {
	p = buf_put_capset(p, e, &in[i].se_capset);
	p = buf_put_string(p, e, in[i].se_name);
        if (p == NULL) {
	    p = save_p;
	    break;
	} else {
	    save_p = p;
	    done++;
	}
    }
    if (p == NULL) {
	Failure(STD_NOSPACE);
    }

    err = sp_mktrans(SP_NTRY, &in[0].se_capset, &hdr, SP_SETLOOKUP,
		     inbuf->sp_buffer, (bufsize) (p - inbuf->sp_buffer),
		     outbuf->sp_buffer, sizeof(outbuf->sp_buffer));

    if (err != STD_OK) {
	Failure(err);
    }

    /* unmarshall results */
    p = outbuf->sp_buffer;
    e = &outbuf->sp_buffer[hdr.h_size];

    for (i = 0; p != NULL && i < done; i++) {
	int16 shortval;
	int32 longval;

	p = buf_get_int16(p, e, &shortval);
	out[i].sr_status = shortval;
	if (out[i].sr_status == STD_OK) {
	    p = buf_get_cap(p, e, &out[i].sr_typecap);
	    p = buf_get_int32(p, e, &longval);
	    out[i].sr_time = longval;
	    p = buf_get_capset(p, e, &out[i].sr_capset);
	}
    }

    if (p == NULL) {
	err = STD_NOSPACE;
    } else {
	err = STD_OK;
	*n_done = done;
    }

failure:
    if (inbuf != NULL) {
	sp_putbuf(inbuf);
    }
    if (outbuf != NULL) {
	sp_putbuf(outbuf);
    }

    return err;
}

#ifdef KERNEL

errstat
sp_setlookup(n, in, out)
int       n;
sp_entry  in[];
sp_result out[];
{
    errstat err;
    int     ndone;

    err = sp_do_setlookup(n, &ndone, in, out);
    if (err == STD_OK && ndone < n) {
	err = STD_NOSPACE;
    }

    return err;
}

#else

/*
 * Outside the kernel, we have implemented two extra features:
 * (1) names "." and ".." are allowed as names to be looked up
 * (2) calls with more than MAX_SETLOOKUP names are broken into smaller
 *     pieces to avoid overflowing in and/or output transaction buffers.
 */


#define dot_or_dotdot(s) (*s == '.' && \
			  (s[1] == '\0' || (s[1] == '.' && s[2] == '\0')))

static void
expand_free(expand_in, size)
sp_entry expand_in[];
int      size;
{
    int i;

    for (i = 0; i < size; i++) {
	cs_free(&expand_in[i].se_capset);
    }
    free((_VOIDSTAR) expand_in);
}

static errstat
expand(in, n, exp_result, namebuf, size)
sp_entry   in[];
int        n;
sp_entry **exp_result;
char	  *namebuf;
size_t     size;
{
    sp_entry *expanded_in = NULL;
    char     *s, *name_ptr, *name_end;
    int       i;
    errstat   err;

    /* If any of the input names are dot or dot-dot, allocate a copy of
     * input request and fill it in with the dot or dot-dot names expanded 
     * so that they refer to a real entry in a (probably different) dir.
     */
    for (i = 0; i < n; i++) {
	s = in[i].se_name;
	if (dot_or_dotdot(s)) {
	    break;
	}
    }

    if (i >= n) { /* no dot or dot-dot */
	*exp_result = in;
	return STD_OK;
    }

    /* Had at least one dot or dot-dot: */
    expanded_in = (sp_entry *) malloc((size_t)(n * sizeof (sp_entry)));
    if (expanded_in == NULL) {
	Failure(STD_NOSPACE);
    }

    name_ptr = namebuf;
    name_end = &namebuf[size - 1];
    *name_end = '\0';

    for (i = 0; i < n; i++) {
	s = expanded_in[i].se_name = in[i].se_name;

	if (dot_or_dotdot(s)) {
	    /* If se_name is "." or "..", and capset is for current dir,
	     * simulate a virtual entry with this name.
	     * We expand se_name to an absolute path, then use sp_traverse
	     * to get the last component and containing directory capset:
	     */
	    capset *origin = &in[i].se_capset;
		
	    if (name_ptr >= name_end) {
		n = i - 1;
		Failure(STD_NOSPACE);
	    }
		
	    if (path_cs_norm(&origin, s, name_ptr, name_end - name_ptr) >= 0) {
		s = name_ptr;
		if (sp_traverse(origin, (const char **) &s,
			        &expanded_in[i].se_capset) == STD_OK)
		{
		    expanded_in[i].se_name = s;
		    name_ptr += strlen(name_ptr) + 1;
		    continue;
		}
	    }
	}
	    
	if (!cs_copy(&expanded_in[i].se_capset, &in[i].se_capset)) {
	    n = i - 1;
	    Failure(STD_NOSPACE);
	}
    }

    /* success: */
    *exp_result = expanded_in;
    return STD_OK;

failure:
    if (expanded_in != NULL) {
	expand_free(expanded_in, n);
    }
    return err;
}

errstat
sp_setlookup(n, in, out)
int       n;
sp_entry  in[];
sp_result out[];
{
    /* If too many entries are being asked for, we might overflow the
     * soap transaction buffers.  Therefore we use sp_do_setlookup to
     * break it up into a series of calls, when necessary.
     * This does break the atomicity semantics for large requests,
     * but typically only the program "dir" uses this feature.
     */
    sp_entry  *expand_in = NULL;
    int	       totdone;
    char       namebuf[PATH_MAX + 1];
    errstat    err;

    if (n < 0) {	/* sanity check */
	return STD_ARGBAD;
    }

    err = expand(in, n, &expand_in, namebuf, sizeof(namebuf));

    totdone = 0;
    while (err == STD_OK && totdone < n) {
	int ndone = 0;

	err = sp_do_setlookup(n - totdone, &ndone,
			      &expand_in[totdone], &out[totdone]);
	if (err == STD_OK) {
	    totdone += ndone;
	}
    }

    if (expand_in != NULL && expand_in != in) {
	expand_free(expand_in, n);
    }

    return err;
}

#endif /* KERNEL */
