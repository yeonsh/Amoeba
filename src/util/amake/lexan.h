/*	@(#)lexan.h	1.3	94/04/07 14:52:37 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define NUM_CHARS	256
/* Used in arrays containing info about all characters */

/* The lexical analyser creates tokens of type 'struct token' */
struct token {
    int t_number;		/* last token found */
    union {
	struct idf *t__idp;	/* ID or REFID  */
	char *t__string;	/* STRING */
	long t__integer;	/* INTEGER */
    } t_misc;
};
#define t_idp		t_misc.t__idp
#define t_string	t_misc.t__string
#define t_integer	t_misc.t__integer

#include "f_info.h"
EXTERN struct f_info file_info;
/* describes current location (file, line) */

EXTERN struct token Tok;
/* 'Tok' contains last token read, including attributes when applicable. */

void HandleNewLine	P((void));
void SetDirTab		P((struct slist *obj_list ));
void read_from		P((char *filename ));
void init_lex		P((void));
void InsertNonIncluded	P((void));
void LexanInsert	P((struct slist *obj_list ));
int  lexan		P((void));
int  read_ahead		P((void));
int  LLmessage		P((int tok_number ));
int  AtEoIF		P((void));
int  InsertIfYouWish	P((void));
int  IsIdent		P((char *str ));

struct object *CurrentFile P((void));
