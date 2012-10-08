#	
#	@(#)Makefile	1.1	91/11/21 13:59:05
#
# This file makes routines that are used everywher.
# 

SITEDIR=..
CC=gcc
OPT=-g
CFLAGS=$(OPT) -I$(SITEDIR)
LDFLAGS=$(OPT)
SHELL=/bin/sh

.SUFFIXES:
.SUFFIXES: .o .c
.c.o:
		$(CC) $(CFLAGS) -c $*.c

default: all

triptrap: all
all: extra.o main.o

extra.o: extra.c extra.h $(SITEDIR)/site.h

main.o: main.c extra.h $(SITEDIR)/site.h


# We don't make `texmf.o', since the TeX and Metafont need to use
# different names, hence they need different .o files.


clean:
	rm -f *.o

veryclean: clean
