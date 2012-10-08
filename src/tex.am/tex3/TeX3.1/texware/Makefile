#	
#	@(#)Makefile	1.1	91/11/21 14:15:37
#
# This file makes DVItype, PLtoTF, TFtoPL, and POOLtype from their
# respective sources.
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


default:	all

all:		triptrap patgen pooltype
triptrap:	dvitype pltotf tftopl


# Routines used everywhere.
# 
commondefines=../amcommon/common.defines
commonh=../amcommon/extra.h
commono=../amcommon/extra.o ../amcommon/main.o

# DVItype:
# 
dvitype:	dvitype.o $(commono)
		ld $(LDFLAGS) -o dvitype \
		$(HEAD) dvitype.o $(commono) $(AMOEBA_LIBS)
#dvitypext.o:	dvitypext.c ${SITEDIR}/site.h
dvitype.o:	dvitype.c $(commonh)
dvitype.c:	dvitype.p $(commondefines)
dvitype.p:	dvitype.web dvitype.ch

# PatGen:
# 
patgen:		patgen.o $(commono)
		ld $(LDFLAGS) -o patgen \
		$(HEAD) patgen.o $(commono) $(AMOEBA_LIBS)
patgen.o:	patgen.c $(commonh)
patgen.c:	patgen.p $(commondefines)
patgen.p:	patgen.web patgen.ch

# PLtoTF:
# 
pltotf:		pltotf.o $(commono)
		ld $(LDFLAGS) -o pltotf \
		$(HEAD) pltotf.o $(commono) $(AMOEBA_LIBS)
pltotf.o:	pltotf.c $(commonh)
		$(CC) $(CFLAGS) -c -Dindex=index_arr -Dlink=link_array pltotf.c
pltotf.c:	pltotf.p $(commondefines)
pltotf.p:	pltotf.web pltotf.ch

# POOLtype:
# 
pooltype:	pooltype.o $(commono)
		ld $(LDFLAGS) -o pooltype \
		$(HEAD) pooltype.o $(commono) $(AMOEBA_LIBS)
pooltype.o:	pooltype.c $(commonh)
pooltype.c:	pooltype.p $(commondefines)
pooltype.p:	pooltype.web pooltype.ch

# TFtoPL:
# 
tftopl:		tftopl.o $(commono)
		ld $(LDFLAGS) -o tftopl \
		$(HEAD) tftopl.o $(commono) $(AMOEBA_LIBS)
tftopl.o:	tftopl.c $(commonh)
		$(CC) $(CFLAGS) -c -Dindex=index_var tftopl.c
tftopl.c:	tftopl.p $(commondefines)
tftopl.p:	tftopl.web tftopl.ch


clean:
		rm -f *.pool *.o dvitype patgen pltotf pooltype tftopl

veryclean:	clean
		rm -f *.p
		rm -f dvitype.c patgen.c pltotf.c pooltype.c tftopl.c
		rm -f dvitype.h patgen.h pltotf.h pooltype.h tftopl.h
		rm -f \#*\# *~ *.bak *.ckp core
