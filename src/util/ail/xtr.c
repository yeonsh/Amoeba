/*	@(#)xtr.c	1.2	94/04/07 14:39:41 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

# include "ail.h"

static struct m_name *decls[200];
static int n_decls = 0;
static Bool used_memcpy = No;

/*
 *	Remember that these declarations still
 *	have to be made using DoDecl()
 */
void
MustDecl(m)
    struct m_name *m;
{
    register int n;
    assert(m != NULL);
    for (n = 0; n < n_decls; ++n) {
	if (decls[n] == m) return;	/* We knew this yet	*/
    }
    if (n_decls >= sizeof(decls)/sizeof(decls[0]))
	fatal("My own decls too small\n");
    decls[n_decls++] = m;
} /* MustDecl */

void
MustDeclMemCpy()
{
    used_memcpy = Yes;
} /* MustDeclMemCpy */

/*
 *	Emit the marshaler declarations.
 *	Reset all state in this module for the next pass
 */
void
DoDecl(h, server)
    handle h;
    Bool server;
{
    if (used_memcpy) {
	used_memcpy = No;
	mypf(h, "#include <string.h>\n");
    }
    if (MarshState.ms_shcast) {
	assert(0); /* Should not be generated; previous version had this bug */
	mypf(h, "static short _pcc; /* Avoid Pcc's signextension bug */\n");
    }
    if (n_decls) {
	do {
	    register struct m_name *mp;
	    mp = decls[--n_decls];
	    assert(mp != NULL);
	    if (server) {
		if (mp->m_svm != NULL)
mypf(h, "#ifndef %s\nextern char *%s(); /* Server marshal */\n#endif\n",
		    mp->m_svm, mp->m_svm);
		if (mp->m_svu != NULL)
mypf(h, "#ifndef %s\nextern char *%s(); /* Server unmarshal */\n#endif\n",
		    mp->m_svu, mp->m_svu);
	    } else {
		if (mp->m_clm != NULL)
mypf(h, "#ifndef %s\nextern char *%s(); /* Client marshal */\n#endif\n",
			mp->m_clm, mp->m_clm);
		if (mp->m_clu != NULL)
mypf(h, "#ifndef %s\nextern char *%s(); /* Client unmarshal */\n#endif\n",
			mp->m_clu, mp->m_clu);
	    }

#if 0
	    if (mp->m_vsize != NULL)
mypf(h, "#ifndef %s\nextern int %s(); /* Size function */\n#endif\n",
		    mp->m_vsize, mp->m_vsize);
#endif

	} while (n_decls > 0);
	mypf(h, "%n");		/* Separate from the function	*/
    }
    assert(n_decls == 0);
} /* DoDecl */
