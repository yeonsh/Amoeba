#	
#	@(#)Makefile	1.1	91/11/21 14:14:59
#
# This file makes TeX in its various incarnations.
#
# The file ../amcommon/texmf.h uses the symbol `TeX' to define certain
# things that are different for TeX and Metafont, hence the addition to
# CFLAGS.
# 

SITEDIR=..
CC=gcc
OPT=-g
CFLAGS=$(OPT) -I$(SITEDIR) -DTeX
LDFLAGS=-lcposix
SHELL=/bin/sh
SIZE=

# The formats we know how to make.
# 
formats=amstex$(SIZE).fmt etex$(SIZE).fmt latex$(SIZE).fmt slitex$(SIZE).fmt tex$(SIZE).fmt \
texinfo$(SIZE).fmt picplus$(SIZE).fmt


makeargs=SITEDIR="$(SITEDIR)" CC="$(CC)" OPT="$(OPT)" LDFLAGS="$(LDFLAGS)" \
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

#putw and getw are NOT in AMOEBA lib -- non-ansi!!!!
putwc=../amcommon/putw.c
putwo=../amcommon/putw.o
getwc=../amcommon/getw.c
getwo=../amcommon/getw.o

# We don't add `$(commono), $(putwo), $(getwo)' here, since they don't depend
# on anything in
# this directory, and we write a dependency for `$(objs)' below.
# 
objs=$(fileioo) tex0.o tex1.o tex2.o tex3.o tex4.o tex5.o tex6.o tex7.o tex8.o
iobjs=iextra.o initex.o $(objs)
vobjs=tex-extra.o itex.o $(objs)
others=$(putwo) $(getwo) $(commono)

default:	all

all:		initex virtex
triptrap:	triptex


initex:		stamp-convert $(iobjs) $(others)
		ld $(LDFLAGS) -o initex \
		$(HEAD) $(iobjs) $(others) $(AMOEBA_LIBS)


virtex:		stamp-convert $(vobjs) $(others)
		ld $(LDFLAGS) -o virtex \
		$(HEAD) $(vobjs) $(others) $(AMOEBA_LIBS)

run-trip:
		rm -f trip.tex
		-ln TeXtrip/trip.tex .
		../texware/pltotf TeXtrip/trip.pl trip.tfm
		-$(SHELL) -c '. ./tripenv; \
                  ./triptex < TeXtrip/trip1.in > /dev/null 2>&1'
		-diff TeXtrip/tripin.log trip.log
		-$(SHELL) -c '. ./tripenv; \
                  ./triptex < TeXtrip/trip2.in > trip.fot'
		-diff TeXtrip/trip.log trip.log
		-diff TeXtrip/trip.fot trip.fot
		$(SHELL) -c '. ./tripenv; \
		  ../texware/dvitype trip.dvi < TeXtrip/dvitype.in > trip.typ'
		-diff TeXtrip/trip.typ trip.typ


# Make format files automatically.
# 
fmts: $(formats)

amstex$(SIZE).fmt: tex$(SIZE).fmt
	./initex \&./tex amstex \\dump

etex$(SIZE).fmt: tex$(SIZE).fmt
	./initex \&./tex eplain \\dump
	mv eplain.fmt etex$(SIZE).fmt
	mv eplain.log etex$(SIZE).log

# Until LaTeX and SLiTeX are updated, we must use a hacked version, that
# sets the new parameters to reasonable values.
# 
latex$(SIZE).fmt:
	-ax -m ast $(bindir)/initex lplain3 \\dump
	ax /bin/mv /lplain3.fmt $(bindir)/latex$(SIZE).fmt
	ax /bin/mv /lplain3.log $(bindir)/latex$(SIZE).log

picplus$(SIZE).fmt: tex$(SIZE).fmt
	./initex \&./tex picplus \\dump

slitex$(SIZE).fmt:
	-ax -m ast $(bindir)/initex splain3 \\dump
	ax /bin/mv /splain3.fmt $(bindir)/slitex$(SIZE).fmt
	ax /bin/mv /splain3.log $(bindir)/slitex$(SIZE).log

tex$(SIZE).fmt:
	-ax -m ast $(bindir)/initex plain \\dump
	ax /bin/mv /plain.fmt $(bindir)/tex$(SIZE).fmt
	ax /bin/mv /plain.log $(bindir)/tex$(SIZE).log

# texinfo changes the escape character from `\' to `@'.
# 
texinfo$(SIZE).fmt: tex$(SIZE).fmt
	./initex \&./tex texinfo @dump


# The `ini_to_trip' script changes texd.h, instead of the change file, so
# retangling is not necessary.  After changing constants, making a
# triptex means making an initex.
# 
triptex:	stamp-trip $(iobjs) $(others)
		make $(makeargs) initex
		mv initex triptex

stamp-trip:	stamp-convert
		rm -f *.o
		$(SHELL) ./ini_to_trip texd.h
		touch stamp-trip

# tangle produces tex.p and tex.pool.
# 
tex.p:		tex.web ctex$(SIZE).ch
		../web/tangle tex.web ctex$(SIZE).ch

# The convert script produces tex[0-8].c, itex.c, coerce.h, and texd.h.
# The (hand-coded) memory.h is included in one of tex?.c files; it isn't
# really necessary to `convert' again if it changes, only to recompile
# those files, but describing that relationship is ugly to do.
# (Furthermore, memory.h is stable.)
# 
stamp-convert:	tex.p memory.h fixcoerce.h $(commondefines)
		$(SHELL) ./convert
		touch stamp-convert

# The (hand-coded) file $(extrac) and the (generated) file itex.c have
# #ifdefs for INITEX, so we compile them differently.
# 
iextra.o:	$(extrac) texd.h $(extrah) $(commonh)
		$(CC) $(CFLAGS) -I. -DINITEX -DINI -c $(extrac)
		mv $(extrao) iextra.o

initex.o:       itex.c texd.h $(extrah) $(commonh)
		$(CC) $(CFLAGS) -DINITEX -c itex.c && mv itex.o initex.o

$(objs):	stamp-convert texd.h
tex-extra.o:	$(extrac) texd.h
		$(CC) $(CFLAGS) -I. -c $(extrac)
		mv $(extrao) tex-extra.o

$(fileioo):	$(fileioc) $(fileioh)
		$(CC) $(CFLAGS) -I. -c $(fileioc)

$(commono):	$(commonc) $(commonh)
		$(CC) $(CFLAGS)  -c $(commonc)

$(putwo):	$(putc)
		$(CC) $(CFLAGS)  -c $(putc)

$(getwo):	$(getwc)
		$(CC) $(CFLAGS)  -c $(getwc)


tex0.o:		tex0.c
tex1.o:		tex1.c
tex2.o:		tex2.c
tex3.o:		tex3.c
tex4.o:		tex4.c
tex5.o:		tex5.c
tex6.o:		tex6.c
tex7.o:		tex7.c
tex8.o:		tex8.c


clean:
		rm -f stamp-* *.o triptex initex virtex
		rm -f texd.h coerce.h *.log *.fmt *.aux
		rm -f 8terminal.tex trip.* tripos.tex dvitype.out

veryclean:	clean
		rm -f tex?.c itex.c tex.p tex.pool texput.*
		rm -f \#*\# *~ *.bak *.ckp core
