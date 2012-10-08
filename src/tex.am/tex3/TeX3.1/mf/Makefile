#	
#	@(#)Makefile	1.1	91/11/21 14:13:14
#
# This file makes Metafont in its various incarnations.
#

SITEDIR=..
CC=gcc
OPT=-g -O
CFLAGS=$(OPT) -I$(SITEDIR)
LDFLAGS=
SHELL=/bin/sh
SIZE=

# The bases we know how to make.
# 
bases=plain$(SIZE).base cmmf$(SIZE).base

# Name of the file that defines the local devices.
# 
localmodes=hayley.mf

# System libraries for window support.
# 
wlibs=
basedir=
bindir=
makeargs=SITEDIR="$(SITEDIR)" CC="$(CC)" OPT="$(OPT)" \
         LDFLAGS="$(LDFLAGS)" wlibs="$(wlibs)" \
	 HEAD="$(HEAD)" AMOEBA_LIBS="$(AMOEBA_LIBS)"

.SUFFIXES:
.SUFFIXES: .o .c
.c.o:
		$(CC) $(CFLAGS) -c $*.c


# Routines used everywhere.
# 
commondefines=../amcommon/common.defines ../amcommon/texmf.defines
commonc=../amcommon/extra.c
commonh=../amcommon/extra.h
commono=../amcommon/extra.o


# Routines used in both TeX and Metafont.
# 
extrac=../amcommon/texmf.c
extrah=../amcommon/texmf.h
extrao=texmf.o

# Routines used in TeX, Metafont, and BibTeX.
# 
fileioc=../amcommon/fileio.c
fileioh=../amcommon/fileio.h
fileioo=fileio.o

#putw.o and getw.o are put in common.  Not in amoeba lib.
putwc=../amcommon/putw.c
getwc=../amcommon/getw.c
putwo=../amcommon/putw.o
getwo=../amcommon/getw.o

windowlib=MFwindow/window.a
libs=$(windowlib) $(wlibs)

# We don't add `$(commono), $(putwo), $(getwo)' here, since they don't depend 
# on anything in
# this directory.
# 
objs=$(fileioo) mf0.o mf1.o mf2.o mf3.o mf4.o mf5.o mf6.o mf7.o mf8.o mf9.o
iobjs=iextra.o inimf.o $(objs)
vobjs=mf-extra.o imf.o $(objs)
others= $(commono) $(putwo) $(getwo)

default:	all

all:		inimf virmf
triptrap:	trapmf



inimf:		stamp-convert $(iobjs) $(windowlib) $(others)
		ld $(LDFLAGS) -o inimf \
		$(HEAD) $(iobjs) $(libs) $(others) $(AMOEBA_LIBS)

virmf:		stamp-convert $(vobjs) $(windowlib) $(others)
		ld $(LDFLAGS) -o virmf \
		$(HEAD) $(vobjs) $(libs) $(others) $(AMOEBA_LIBS)

run-trap:
		rm -f trap.mf
		-ln MFtrap/trap.mf .
		-$(SHELL) -c '. ./trapenv; \
                  ./trapmf < MFtrap/trap1.in > /dev/null 2>&1'
		-diff MFtrap/trapin.log trap.log
		-$(SHELL) -c '. ./trapenv; \
                  ./trapmf < MFtrap/trap2.in > trap.fot'
		-diff MFtrap/trap.log trap.log
		-diff MFtrap/trap.fot trap.fot
		../texware/tftopl trap.tfm trap.pl
		-diff MFtrap/trap.pl trap.pl
		$(SHELL) -c '. ./trapenv; \
		  ../mfware/gftype -m -i trap.72270gf > trap.typ'
		-diff MFtrap/trap.typ trap.typ


# Make base files automatically.
# 
bases:	$(bases)

mf$(SIZE).base:
	-ax -m ast $(bindir)/inimf 'plain; input $(localmodes); dump'
	ax /bin/mv /plain.base $(bindir)/mf$(SIZE).base
	ax /bin/mv /plain.log $(bindir)/mf$(SIZE).log

#cmmf.base: mf.base
#	./inimf \&./mf cm dump
#	mv cm.base cmmf.base
#	mv cm.log cmmf.log

cmmf$(SIZE).base: mf$(SIZE).base
	-ax -m ast $(bindir)/inimf \&$(bindir)/mf$(SIZE) cmbase dump
	ax /bin/mv /cmbase.base $(bindir)/cmmf$(SIZE).base
	ax /bin/mv /cmbase.log $(bindir)/cmmf$(SIZE).log

# The `ini_to_trap' script changes mfd.h, instead of the change file, so
# retangling is not necessary.  After changing constants, making a
# trapmf means making an inimf.  We don't actually have to make the
# window library when making a trapmf, but it shouldn't hurt.
# 
trapmf:		stamp-trap $(iobjs) $(others)
		make $(makeargs) inimf
		mv inimf trapmf

stamp-trap:	stamp-convert
		rm -f *.o
		$(SHELL) ./ini_to_trap mfd.h
		touch stamp-trap

# tangle produces mf.p and mf.pool.
# 
mf.p:		mf.web cmf$(SIZE).ch
		../web/tangle mf.web cmf$(SIZE).ch

# The convert script produces mf[0-9].c, imf.c, coerce.h, and mfd.h.
# The (hand-coded) memory.h is included in the mf?.c files; it isn't
# really necessary to `convert' again if it changes, only to recompile
# those files, but describing that relationship is ugly to do.
# (Furthermore, memory.h is stable.)
# 
stamp-convert:	mf.p memory.h fixcoerce.h $(commondefines)
		$(SHELL) ./convert
		touch stamp-convert

# The (hand-coded) file $(extrac) and the (generated) file imf.c have
# #ifdefs for INIMF, so we have to compile them differently.
# 
iextra.o:	$(extrac) mfd.h $(extrah) $(commonh)
		$(CC) $(CFLAGS) -I. -DINIMF -DINI -c $(extrac)
		mv $(extrao) iextra.o

inimf.o:	imf.c mfd.h $(extrah) $(commonh)
		$(CC) $(CFLAGS) -DINIMF -c imf.c && mv imf.o inimf.o

$(objs):	stamp-convert mfd.h

mf-extra.o:	$(extrac) mfd.h
		$(CC) $(CFLAGS) -I. -c $(extrac)
		mv $(extrao) mf-extra.o

$(fileioo):	$(fileioc) $(fileioh)
		$(CC) $(CFLAGS) -I. -c $(fileioc)

$(commono):	$(commonc) $(commonh)
		$(CC) $(CFLAGS) -I. -c $(commonc)

$(putwo):	$(putwc)
		$(CC) $(CFLAGS) -I. -c $(putwc)

$(getwo):	$(getwc)
		$(CC) $(CFLAGS) -I. -c $(getwc)


mf0.o:		mf0.c
mf1.o:		mf1.c
mf2.o:		mf2.c
mf3.o:		mf3.c
mf4.o:		mf4.c
mf5.o:		mf5.c
mf6.o:		mf6.c
mf7.o:		mf7.c
mf8.o:		mf8.c
mf9.o:		mf9.c

$(windowlib):
		cd MFwindow; make $(makeargs)

clean:
		rm -f stamp-* *.o trapmf inimf virmf
		rm -f mfd.h coerce.h *.log *.base
		rm -f trap.*
		cd MFwindow; make clean

veryclean:	clean
		rm -f mf?.c imf.c mf.p mf.pool mfput.*
		rm -f \#*\# *~ *.bak *.ckp core
		cd MFwindow; make veryclean
