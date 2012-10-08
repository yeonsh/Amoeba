/*	@(#)tr.c	1.2	91/11/14 14:13:42 */
/* tr - translate characters		Author: Michiel Huisjes */
/* Usage: tr [-cds] [string1 [string2]]
 * 	c: take complement of string1
 * 	d: delete input characters coded string1
 * 	s: squeeze multiple output characters of string2 into one character
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SIZE	1024
#define ASCII		0377

typedef char BOOL;
#define TRUE	1
#define FALSE	0

#define NIL_PTR		((char *) 0)

BOOL com_fl, del_fl, sq_fl;

unsigned char output[BUFFER_SIZE], input[BUFFER_SIZE];
unsigned char vector[ASCII + 1];
BOOL invec[ASCII + 1], outvec[ASCII + 1];

short in_index, out_index;

main(argc, argv)
int argc;
char *argv[];
{
  register char *aptr;
  register unsigned char *ptr;
  int index = 1;
  short i;

  if (argc > 1 && argv[index][0] == '-') {
	for (aptr = &argv[index][1]; *aptr; aptr++) {
		switch (*aptr) {
		case 'c' :
			com_fl = TRUE;
			break;
		case 'd' :
			del_fl = TRUE;
			break;
		case 's' :
			sq_fl = TRUE;
			break;
		default :
			fprintf(stderr, "Usage: tr [-cds] [string1 [string2]].\n");
			exit(1);
		}
	}
	index++;
  }
  
  for (i = 0; i <= ASCII; i++) {
	vector[i] = i;
	invec[i] = outvec[i] = FALSE;
  }

  if (argv[index] != NIL_PTR) {
	expand(argv[index++], input);
	if (com_fl)
		complement(input);
	if (argv[index] != NIL_PTR)
		expand(argv[index], output);
	if (argv[index] != NIL_PTR)
		map(input, output);
	for (ptr = input; *ptr; ptr++)
		invec[*ptr] = TRUE;
	for (ptr = output; *ptr; ptr++)
		outvec[*ptr] = TRUE;
  }
  convert();
  /*NOTREACHED*/
}

convert()
{
  short read_chars = 0;
  short c, coded;
  short last = -1;

  for (; ;) {
	if (in_index == read_chars) {
		if ((read_chars = read(0, input, BUFFER_SIZE)) <= 0) {
			if (write(1, output, out_index) != out_index) {
				fprintf(stderr, "Bad write\n");
				exit(1);
			} else {
				exit(0);
			}
		}
		in_index = 0;
	}
	c = input[in_index++];
	coded = vector[c];
	if (del_fl && invec[c])
		continue;
	if (sq_fl && last == coded && outvec[coded])
		continue;
	output[out_index++] = last = coded;
	if (out_index == BUFFER_SIZE) {
		if (write(1, output, out_index) != out_index) {
			fprintf(stderr, "Bad write\n");
			exit(1);
		}
		out_index = 0;
	}
  }

  /* NOTREACHED */
}

map(string1, string2)
register unsigned char *string1, *string2;
{
  unsigned char last;

  while (*string1) {
	if (*string2 == '\0')
		vector[*string1] = last;
	else
		vector[*string1] = last = *string2++;
	string1++;
  }
}

badsyntax()
{
  fprintf(stderr, "bad syntax: string may only consist of one or more of\n");
  fprintf(stderr, "range,  e.g.:  [a-z]\n");
  fprintf(stderr, "escape, e.g.:  \\012 or \\[\n");
  fprintf(stderr, "other,  e.g.:  A or ( or 1\n");
  exit(1);
}
	
expand(arg, buffer)
register char *arg;
register unsigned char *buffer;
{
  int i, ac;

  while (*arg) {
	if (*arg == '\\') {
		arg++;
		i = ac = 0;
		if (*arg >= '0' && *arg <= '7') {
			do {
				ac = (ac << 3) + *arg++ - '0';
				i++;
			} while (i < 4 && *arg >= '0' && *arg <= '7');
			*buffer++ = ac;
		}
		else if (*arg != '\0') {
			*buffer++ = *arg;
		} else {
			badsyntax();
		}
	}
	else if (*arg == '[') {
		arg++;
		/* parse rest of the range; it should be of the form 'a-z]' */
		if ((i = *arg++) == '\0') {
			badsyntax();
		}
		if (*arg++ != '-') {
			badsyntax();
		}
		if ((ac = *arg++) == '\0') {
			badsyntax();
		}
		if (*arg++ != ']') {
			badsyntax();
		} 
		while (i <= ac)
			*buffer++ = i++;
	}
	else
		*buffer++ = *arg++;
  }
}

complement(buffer)
unsigned char *buffer;
{
  register unsigned char *ptr;
  register short i, index;
  unsigned char conv[ASCII + 2];

  index = 0;
  for (i = 1; i <= ASCII; i++) {
	for (ptr = buffer; *ptr; ptr++)
		if (*ptr == i)
			break;
	if (*ptr == '\0')
		conv[index++] = i & ASCII;
  }
  conv[index] = '\0';
  strcpy((char *) buffer, (char *) conv);
}
