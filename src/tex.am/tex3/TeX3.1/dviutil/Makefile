#	
#	@(#)Makefile	1.1	91/11/21 14:01:35
#
# This file makes DVIcopy.
#

SITEDIR=..
CC=gcc
OPT=-g
CFLAGS=$(OPT) -I$(SITEDIR)
LDFLAGS=$(OPT)
SHELL=/bin/sh

.SUFFIXES:
.SUFFIXES: .o .c .p .ch
.p.c:
		$(SHELL) ../common/convert $*.p $*.c
.ch.p:
		../web/tangle $*.web $*.ch
.c.o:
		$(CC) $(CFLAGS) -c $*.c


default:        all

all:            dvicopy


# Routines used everywhere.
#
commondefines=../amcommon/common.defines
commonh=../amcommon/extra.h
commonc=../amcommon/extra.c ../amcommon/extra.o
commono=../amcommon/extra.o ../amcommon/main.o

# DVIcopy:
#
dvicopy:        dvicopy.o $(commono)
		ld $(LDFLAGS) -o dvicopy \
		$(HEAD) dvicopy.o $(commono) $(AMOEBA_LIBS)
dvicopy.o:      dvicopy.c $(commonh)
dvicopy.c:      dvicopy.p $(commondefines)
dvicopy.p:      dvicopy.web dvicopy.ch

clean:
		rm -f *.pool *.o dvicopy

veryclean:      clean
		rm -f *.p
		rm -f dvicopy.c
		rm -f dvicopy.h
		rm -f \#*\# *~ *.bak *.ckp core
