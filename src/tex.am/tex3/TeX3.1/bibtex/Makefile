#	
#	@(#)Makefile	1.1	91/11/21 13:56:59
#
# This file makes Bibtex.
#

SITEDIR=..
CC=gcc
OPT=-g
CFLAGS=$(OPT) -I$(SITEDIR) -DBibTeX
LDFLAGS=$(OPT)
SHELL=/bin/sh

.SUFFIXES:
.SUFFIXES: .o .c .p .ch
.p.c:
		$(SHELL) ./convert $*.p $*.c
.ch.p:
		../web/tangle $*.web $*$(SIZE).ch
.c.o:
		$(CC) $(CFLAGS) -c $*.c

# Routines used everywhere.
# 
commondefines=../amcommon/common.defines ../amcommon/texmf.defines
commonh=../amcommon/extra.h
commono=../amcommon/extra.o ../amcommon/main.o

# Routines also used in TeX and Metafont (only).
# 
extrac=../amcommon/fileio.c
extrah=../amcommon/fileio.h
extrao=fileio.o


default:	all

all:		bibtex


bibtex:		bibtex.o $(commono) $(extrao)
		ld $(LDFLAGS) -o bibtex \
		$(HEAD) bibtex.o $(commono) $(extrao) $(AMOEBA_LIBS)
bibtex.o:	bibtex.c $(commonh) $(extrah)
bibtex.c:	bibtex.p $(commondefines)
bibtex.p:	bibtex.web bibtex$(SIZE).ch
		../web/tangle bibtex.web bibtex$(SIZE).ch
$(extrao):	$(extrac) $(extrah)
		$(CC) $(CFLAGS) -I. -c $(extrac)


clean:
		rm -f bibtex.pool *.o bibtex

veryclean:	clean
		rm -f bibtex.p bibtex.c bibtex.h coerce.h
		rm -f \#*\# *~ *.bak *.ckp core
