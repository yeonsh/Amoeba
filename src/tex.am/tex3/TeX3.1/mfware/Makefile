#	
#	@(#)Makefile	1.1	91/11/21 14:14:23
#
# This file makes GFtoDVI, GFtoPK, GFtype, mft, PKtoGF, and PKtype from
# their respective sources.
#

SITEDIR=..
CC=gcc
OPT=-g
CFLAGS=$(OPT) -I$(SITEDIR)
LDFLAGS=-lcposix
SHELL=/bin/sh

.SUFFIXES:
.SUFFIXES: .o .c .p .ch
.p.c:
		$(SHELL) ../common/convert $*.p $*.c
.ch.p:
		../web/tangle $*.web $*.ch
.c.o:
		$(CC) $(CFLAGS) -c $*.c

# Routines used everywhere.
# 
commondefines=../amcommon/common.defines
commonh=../amcommon/extra.h
commono=../amcommon/extra.o ../amcommon/main.o


default:	all

all:		triptrap gftodvi gftopk mft
triptrap:	gftype


# GFtoDVI:
# 
gftodvi:	gftodvi.o $(commono)
		ld $(LDFLAGS) -o gftodvi \
		$(HEAD) gftodvi.o $(commono) $(AMOEBA_LIBS)
gftodvi.o:	gftodvi.c $(commonh) memory.h
gftodvi.c:	gftodvi.p $(commondefines)
gftodvi.p:	gftodvi.web gftodvi.ch

# GFtoPK:
# 
gftopk:		gftopk.o $(commono)
		ld $(LDFLAGS) -o gftopk \
		$(HEAD) gftopk.o $(commono) $(AMOEBA_LIBS)
gftopk.o:	gftopk.c $(commonh)
gftopk.c:	gftopk.p $(commondefines)
gftopk.p:	gftopk.web gftopk.ch

# GFtype:
# 
gftype:		gftype.o $(commono)
		ld $(LDFLAGS) -o gftype \
		$(HEAD) gftype.o $(commono) $(AMOEBA_LIBS) 
gftype.o:	gftype.c $(commonh)
gftype.c:	gftype.p $(commondefines)
gftype.p:	gftype.web gftype.ch

# MFT:
# 
mft:		mft.o $(commono)
		ld $(LDFLAGS) -o mft \
		$(HEAD) mft.o $(commono) $(AMOEBA_LIBS)
mft.o:		mft.c $(commonh)
mft.c:		mft.p $(commondefines)
mft.p:		mft.web mft.ch


clean:
		rm -f *.pool *.o gftodvi gftype gftopk mft

veryclean:	clean
		rm -f *.p
		rm -f gftodvi.c gftype.c gftopk.c mft.c
		rm -f gftodvi.h gftype.h gftopk.h mft.h
		rm -f \#*\# *~ *.bak *.ckp core




