/*	@(#)mkproto.h	1.1	91/04/09 17:05:10 */
#ifdef __STDC__
# define	P(s) s
#else
# define P(s) ()
#endif


/* mkproto.c */
Word *word_alloc P((char *s ));
void word_free P((Word *w ));
int List_len P((Word *w ));
Word *word_append P((Word *w1 , Word *w2 ));
int foundin P((Word *w1 , Word *w2 ));
void addword P((Word *w , char *s ));
Word *typelist P((Word *p ));
void typefixhack P((Word *w ));
int ngetc P((FILE *f ));
int fnextch P((FILE *f ));
int nextch P((FILE *f ));
int getsym P((char *buf , FILE *f ));
int skipit P((char *buf , FILE *f ));
Word *getparamlist P((FILE *f ));
void emit P((Word *wlist , Word *plist , long startline ));
void getdecl P((FILE *f ));
void main P((int argc , char **argv ));
void Usage P((void ));

#undef P
