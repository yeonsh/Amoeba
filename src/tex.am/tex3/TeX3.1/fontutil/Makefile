#	
#	@(#)Makefile	1.1	91/11/21 14:02:15
#
# This file makes some font utilities (that are not part of MFware)
# from their respective sources.
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

all:		pktogf pktype vftovp vptovf


# Routines used everywhere.
# 
commondefines=../amcommon/common.defines
commonh=../amcommon/extra.h
commono=../amcommon/extra.o ../amcommon/main.o

# PKtoGF:
# 
pktogf:		pktogf.o $(commono)
		ld $(LDFLAGS) -o pktogf \
		$(HEAD) pktogf.o $(commono) $(AMOEBA_LIBS)
pktogf.o:	pktogf.c $(commonh)
pktogf.c:	pktogf.p $(commondefines)
pktogf.p:	pktogf.web pktogf.ch

# PKtype:
# 
pktype:		pktype.o $(commono)
		ld $(LDFLAGS) -o pktype \
		$(HEAD) pktype.o $(commono) $(AMOEBA_LIBS)
pktype.o:	pktype.c $(commonh)
pktype.c:	pktype.p $(commondefines)
pktype.p:	pktype.web pktype.ch

# VFtoVP:
# 
vftovp:	vftovp.o $(commono)
		ld $(LDFLAGS) -o vftovp \
		$(HEAD) vftovp.o $(commono) $(AMOEBA_LIBS)
vftovp.o:	vftovp.c $(commonh)
		$(CC) $(CFLAGS) -c -Dindex=index_var -Dlink=link_array vftovp.c
vftovp.c:	vftovp.p $(commondefines)
vftovp.p:	vftovp.web vftovp.ch

# VPtoVF:
# 
vptovf:		vptovf.o $(commono)
		ld $(LDFLAGS) -o vptovf \
		$(HEAD) vptovf.o $(commono) $(AMOEBA_LIBS)
vptovf.o:	vptovf.c $(commonh)
		$(CC) $(CFLAGS) -c -Dindex=index_arr -Dlink=link_array vptovf.c
vptovf.c:	vptovf.p $(commondefines)
vptovf.p:	vptovf.web vptovf.ch

clean:
		rm -f *.pool *.o pktogf pktype vftovp vptovf

veryclean:	clean
		rm -f *.p
		rm -f pktogf.c pktype.c vftovp.c vptovf.c
		rm -f pktogf.h pktype.h vftovp.h vptovf.h
		rm -f \#*\# *~ *.bak *.ckp core
