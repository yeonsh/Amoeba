# Overall Makefile for making TeX, Metafont, and friends, using WEB to C.
#	
#	@(#)makefile	1.1	91/11/21 14:16:52
#
# 
#
# TeX 3.1 + Metafont 2.7 + web2c a.
# 
version=5.8a
ax=/usr/local/ambin/ax

#Added for amoeba
HEAD=/usr/proj/amwork/conf/amoeba/mc68000.sun/lib/head/head.o
#HEAD=
# Directory that site.h is in relative to subdirectories, i.e., `..'
# means the directory containing this Makefile.
SITEDIR=..

# Directory that site.h is in relative to the current directory, i.e.,
# `.' means the directory containing this Makefile.
siteh=./site.h
# Default C compiler.
CC=cc
# ***IMPORTANT!!!!**** This is 16 for small TeX and 32 for large TeX.  
SIZE=32

DEFINES=-DAMOEBA -Drindex=strrchr -Dindex=strchr -DSIZE$(SIZE)
# Default flags to give the C compiler.  (Don't define CFLAGS directly.)
#OPT=-O
OPT=-O  -f68881 -I/usr/proj/amwork/src/h/posix -I/usr/proj/amwork/src/h/posix/sys -I/usr/proj/amwork/src/h -I/usr/proj/amwork/src/h/posix/machdep/mc68000 -I/usr/proj/amwork/src/h/ajax -I/usr/proj/amwork/conf/amoeba/mc68000.sun/X $(DEFINES)


# Default flags to give to the loader, including libraries.  (LDFLAGS is
# given last on the command line.)
#LDFLAGS=
LDFLAGS=-Bstatic -e start 

AMOEBA_LIBS=/usr/proj/amwork/conf/amoeba/mc68000.sun/lib/ajax/libajax.a /usr/proj/amwork/conf/amoeba/mc68000.sun/lib/amoeba/libamoeba.a 
#AMOEBA_LIBS=

AMOEBA_XLIBS=/usr/proj/amwork/conf/amoeba/mc68000.sun/X/lib/Xt/libXt.a /usr/proj/amwork/conf/amoeba/mc68000.sun/X/extensions/lib/libXext.a /usr/proj/amwork/conf/amoeba/mc68000.sun/X/lib/X/libX11.a 

# Make sure we use the sh.  If your sh is broken, you might try bash,
# the GNU replacement.
SHELL=/bin/sh

# The yacc program to use on web2c/web2c.yacc.
YACC=yacc

# The lex program to use on web2c/web2c.lex.
LEX=lex
# -ll for lex, -lfl for flex.
LEXLIB=-ll

# Need -D_POSIX_SOURCE here, perhaps.
LEXCFLAGS=

# Support libraries for Metafont.
x10lib=-lX
x11lib=-lXt -lX11 -lXext
sunlib=-lsuntool -lsunwindow -lpixrect

# This line should select the libraries that match what you said in site.h.
#wlibs=${x11lib}
wlibs=${AMOEBA_XLIBS}

# These lines define the format and base files that `make fmts' and
# `make bases' will try to make, and `make install' will try to install.
#formats=tex.fmt latex.fmt slitex.fmt amstex.fmt etex.fmt picplus.fmt 
# texinfo.fmt
formats=tex$(SIZE).fmt latex$(SIZE).fmt slitex$(SIZE).fmt
# amstex$(SIZE).fmt
bases=mf$(SIZE).base cmmf$(SIZE).base

# The name of the file that defines your local devices, for use with
# Metafont.  (Only relevant during `make bases'.)
localmodes=VUlocal

# You may need INSTALL=cp on system V, if you don't have GNU install. 
# Or you can install the programs by hand.
# $(fileinstall) is used for the format and base files, man files, and
# other non-executables, and $(programinstall) is used for
# the executables.
INSTALL=ainstall
fileinstall=tob
proginstall=$(INSTALL)

# These pathnames are used to customize the manual pages with local
# directory names; some of them are used in `make install'.  They should
# match the system directories in the paths defined in `./site.h'.
bindir=/super/util
manext=1
mandir=/usr/local/man/man$(manext)

texlibdir=/super/module/tex
texpooldir=$(texlibdir)
texinputdir=$(texlibdir)/inputs
texfontdir=$(texlibdir)/fonts/tfm
formatdir=$(texlibdir)/formats

mflibdir=/super/module/mf
mfpooldir=$(mflibdir)
mfinputdir=$(mflibdir)/inputs
basedir=$(mflibdir)/bases

# Where the Computer Modern sources are.
# (The man pages claim that you have installed the standard utility
# sources, like `grayf.mf' and `expr.mf', somewhere in $(mfinputdir).)
# cmsources=/usr/local/src/other/TeX+MF/typefaces/cm
cmsources=/super/module/mf/inputs

default: all


# Stuff below here probably doesn't need to be changed.
# 
alldirs=tex mf texware mfware fontutil dviutil bibtex 
webdir=web
triptrapdirs=tex texware mf mfware

#To make executables to run on amoeba.
makeargs=SITEDIR="$(SITEDIR)" CC="$(CC)" OPT="$(OPT)" \
SHELL="$(SHELL)" LDFLAGS="$(LDFLAGS)" wlibs="$(wlibs)" HEAD="$(HEAD)" \
AMOEBA_LIBS="$(AMOEBA_LIBS)" SIZE="$(SIZE)"
#To make executables, used in making TeX, which run on Unix.
makeargslite=SITEDIR="$(SITEDIR)" CC="$(CC)" OPT="-O -DSIZE$(SIZE)" \
SHELL="$(SHELL)" LDFLAGS="" wlibs="$(wlibs)"


commonsources=common/extra.c common/main.c 
amcommonsources=amcommon/extra.c amcommon/main.c amcommon/putw.c amcommon/getw.c
web2csources=web2c/fixwrites.c web2c/regfix.c web2c/splitup.c \
             web2c/web2c.c web2c/web2c.lex web2c/web2c.yacc \
             web2c/web2c.h
web2cprograms=web2c/web2c web2c/regfix web2c/splitup web2c/fixwrites

triptrap: stamp-tangle
	for name in $(triptrapdirs);					\
        do								\
          (cd $${name}; make $(makeargs) triptrap);			\
        done  

run-triptrap: run-trip run-trap

clean-triptrap:
	cd tex; make veryclean
	cd mf; make veryclean

all:	stamp-web2c stamp-common stamp-amcommon
	for name in $(webdir);						\
        do								\
          (cd $${name}; echo $${name}; make $(makeargslite) all);	\
        done
	for name in $(alldirs);						\
        do								\
          (cd $${name}; echo $${name}; make $(makeargs) all);		\
        done
	

TeX:	stamp-web2c stamp-tangle stamp-amcommon
	cd tex; make $(makeargs) all


run-trip:
	cd tex; make run-trip

MF:	stamp-web2c stamp-tangle stamp-amcommon
	cd mf; make $(makeargs) all

run-trap:
	cd mf; make run-trap


# These targets must be up to date for anything to be made.
# 
stamp-web2c: $(web2cprograms)
	touch stamp-web2c

$(web2cprograms): $(web2csources)
	cd web2c; \
          make $(makeargslite) YACC="$(YACC)" LEX="$(LEX)" LEXLIB="$(LEXLIB)" all

stamp-common: $(commonsources) $(siteh)
	cd common; make $(makeargslite) all
	touch stamp-common

stamp-amcommon: $(amcommonsources) $(siteh)
	cd amcommon; make $(makeargs) all
	touch stamp-amcommon

stamp-tangle: stamp-web2c stamp-common
	cd web; make $(makeargslite) tangle
	touch stamp-tangle


manpages:
	cd man; make SITEDIR=$(SITEDIR)					\
		bindir=$(bindir) texlibdir=$(texlibdir)			\
		texpooldir=$(texpooldir) formatdir=$(formatdir)		\
		texinputdir=$(texinputdir) texfontdir=$(texfontdir)	\
		mflibdir=$(mflibdir) mfpooldir=$(mfpooldir)		\
		basedir=$(basedir) mfinputdir=$(mfinputdir)		\
                cmsources=$(cmsources)					\
                all

bases: stamp-bases
stamp-bases:
	cd mf; MFPOOL=$(mfpooldir); export MFPOOL; \
          make $(makeargs) bases="$(bases)" localmodes="$(localmodes)" \
	basedir="$(basedir)" bindir="$(bindir)" bases
	touch stamp-bases

formats: stamp-formats
stamp-formats:
	cd tex; TEXPOOL=$(texpooldir); export TEXPOOL; \
          make $(makeargs) formats="$(formats)" formatdir="$(formatdir)" \
	bindir="$(bindir)" fmts
	touch stamp-formats


# Installation targets.
# 
install-manpages:
	cd man; make mandir=$(mandir) INSTALL="$(fileinstall)" \
          SITEDIR="$(SITEDIR)" manext=$(manext) install


install:
	$(proginstall) tex/initex $(bindir)/initex
	$(proginstall) tex/virtex $(bindir)/virtex
	$(proginstall) mf/inimf $(bindir)/inimf
	$(proginstall) mf/virmf $(bindir)/virmf
#NOTE: tangle and weave are compiled for unix -- not amoeba!!!
#	$(proginstall) web/tangle $(bindir)/tangle
#	$(proginstall) web/weave $(bindir)/weave
	$(proginstall) texware/dvitype $(bindir)/dvitype
	$(proginstall) texware/pltotf $(bindir)/pltotf
	$(proginstall) texware/tftopl $(bindir)/tftopl
	$(proginstall) mfware/gftodvi $(bindir)/gftodvi
	$(proginstall) mfware/gftopk $(bindir)/gftopk
	$(proginstall) mfware/gftype $(bindir)/gftype
	$(proginstall) mfware/mft $(bindir)/mft
	$(proginstall) fontutil/pktogf $(bindir)/pktogf
	$(proginstall) fontutil/pktype $(bindir)/pktype
	$(proginstall) fontutil/vftovp $(bindir)/vftovp
	$(proginstall) fontutil/vptovf $(bindir)/vptovf
	$(proginstall) bibtex/bibtex $(bindir)/bibtex$(SIZE)
	$(proginstall) dviutil/dvicopy $(bindir)/dvicopy
	-ax /bin/sh -c "echo '#!/bin/sh' > $(bindir)/bibtex"
	-ax /bin/sh -c "echo 'ax -m ast /public/util/bibtex$(SIZE) \$${1}' >> $(bindir)/bibtex"
	-ax /bin/sh -c "if /bin/test ! -d $(texpooldir); then /bin/mkdir $(texpooldir); fi"
	-ax /bin/sh -c "if /bin/test -f $(texpooldir)/tex.pool; then /public/util/del -d $(texpooldir)/tex.pool; fi"
	if test -s tex/tex.pool; then $(fileinstall) tex/tex.pool $(texpooldir)/tex.pool; fi

	-ax /bin/sh -c "if /bin/test ! -d $(mfpooldir); then /bin/mkdir $(mfpooldir); fi"
	-ax /bin/sh -c "if /bin/test -f $(mfpooldir)/mf.pool; then /public/util/del -d $(mfpooldir)/mf.pool; fi"
	if test -s mf/mf.pool; then $(fileinstall) mf/mf.pool $(mfpooldir)/mf.pool; fi


install-formats: stamp-formats
	-ax /bin/sh -c "if /bin/test ! -d $(formatdir); then /bin/mkdir $(formatdir); fi"
	-for f in $(formats) ;						\
	do								\
	  $(ax) /public/util/del -d $(bindir)/`basename $$f .fmt`;	\
          $(ax) /bin/ln $(bindir)/virtex $(bindir)/`basename $$f .fmt`;	\
	  $(ax) /bin/sh -c "echo '#!/bin/sh' > $(bindir)/`basename $$f $(SIZE).fmt`"; \
	  $(ax) /bin/sh -c "echo 'ax -m ast /public/util/`basename $$f .fmt` \$${1}' >> $(bindir)/`basename $$f $(SIZE).fmt`"; \
	  $(ax) /public/util/del -d $(formatdir)/$$f;			\
          $(ax) /bin/mv $(bindir)/$$f $(formatdir)/$$f;			\
          $(ax) /bin/mv $(bindir)/`basename $$f .fmt`.log $(formatdir)/`basename $$f .fmt`.log;	\
	done
	-ax /public/util/del -d $(formatdir)/plain$(SIZE).fmt; 
	ax /bin/ln $(formatdir)/tex$(SIZE).fmt $(formatdir)/plain$(SIZE).fmt
	ax /bin/mv $(bindir)/initex $(bindir)/initex$(SIZE)
	ax /bin/mv $(bindir)/virtex $(bindir)/virtex$(SIZE)


install-bases: stamp-bases
	-ax /bin/sh -c "if /bin/test ! -d $(basedir); then /bin/mkdir $(basedir); fi"
	-for f in $(bases) ;						\
	do								\
	  $(ax) /public/util/del -d $(bindir)/`basename $$f .base`;	\
	  $(ax) /bin/ln $(bindir)/virmf $(bindir)/`basename $$f .base`; \
	  $(ax) /bin/sh -c "echo '!#/bin/sh' > $(bindir)/`basename $$f $(SIZE).base`"; \
	  $(ax) /bin/sh -c "echo 'ax -m ast /public/util/`basename $$f .base` \$${1}' >> $(bindir)/`basename $$f $(SIZE).base`"; \
	  $(ax) /public/util/del -d $(basedir)/$$f;			\
          $(ax) /bin/mv $(bindir)/$$f $(basedir)/$$f;			\
          $(ax) /bin/mv $(bindir)/`basename $$f .base`.log $(basedir)/`basename $$f .base`.log;	\
	done
	-ax /public/util/del -d $(basedir)/plain$(SIZE).base; 
	ax /bin/ln $(basedir)/mf$(SIZE).base $(basedir)/plain$(SIZE).base
	ax /bin/mv $(bindir)/inimf $(bindir)/inimf$(SIZE)
	ax /bin/mv $(bindir)/virmf $(bindir)/virmf$(SIZE)


# Need GNU tar here.
# 
web2c-dist: web/tangleboot.pas
	@echo -n "check -linet; xt vs. xlib.;tangleboot;modes file;"
	@echo "big{tex,mf,bib}.ch; trie change"
	cd ..;								\
	  find src-$(version) \! -name '*.web' -type f -print > /tmp/files; \
	  tar czfT web2c$(version).tarZ /tmp/files;			\
          ls -ld web*

web-dist: web/tangleboot.pas
	cd ..;								\
          find src-$(version) -name '*.web' -print > /tmp/webfest;	\
          tar czfT web-$(version).tar.Z /tmp/webfest;			\
          ls -ld web*

web/tangleboot.pas:
	cd web; make tangle.p; cp tangle.p tangleboot.pas; make veryclean

clean:
	rm -f stamp-*
	for name in web2c common amcommon web $(alldirs);				\
        do								\
          (cd $${name}; make clean);					\
        done

veryclean:
	rm -f stamp-*
	for name in web2c common amcommon man web $(alldirs);			\
        do								\
          (cd $${name}; make veryclean);				\
        done
	rm -f \#*\# *~ *.bak *.ckp core

.PHONY: default triptrap run-triptrap clean-triptrap all TeX run-trip \
	MF run-trap manpages bases formats install-manpages install \
        install-formats install-bases web2c-dist web-dist clean veryclean
