#	
#	@(#)Makefile	1.2	91/11/27 13:27:20
#
#
# Makefile for dvi previewer.
#
#
DEFAULT_FONT_PATH=/public/module/tex/fonts/raster
DEFAULT_FONT_SIZES=300:328.6:360:432:518.4:622:746.4
DEFINES=-DMSBITFIRST \
  -DBMSHORT -DBUTTONS -DA4  -DAMOEBA -Dx11 \
  -Drindex=strrchr -Dindex=strchr 
FONTDEFINES=-DDEFAULT_FONT_PATH=\"$(DEFAULT_FONT_PATH)\" \
  -DDEFAULT_FONT_SIZES=\"$(DEFAULT_FONT_SIZES)\" \
  -DDEFAULT_SUBDIR_PATH=\"\"
FONTFORMATS_C=gf.c pk.c pxl.c
FONTFORMATS_O=gf.o pk.o pxl.o
LIBDIR=/usr/custom/x11/lib
#LIBDIR=/tuna_a/x11r4/lib
INCDIR=/usr/proj/amwork/src/h
#INCDIR=/tuna_a/x11r4/include
XMULIB=
#XMULIB=-lXmu
EXTENSIONLIB=-lXext
DESTDIR=/usr/local/x11
BINDIR=$(DESTDIR)/bin
MANDIR=$(DESTDIR)/man
INCLUDES=-I$(INCDIR) -I$(INCDIR)/posix -I$(INCDIR)/posix/sys -I$(INCDIR)/posix/machdep/mc68000 -I$(INCDIR)/ajax -I/usr/proj/amwork/conf/amoeba/mc68000.sun/X
CFLAGS=-g -O -f68881 $(INCLUDES) $(DEFINES)
#LIBS=-L$(LIBDIR) -lXaw $(XMULIB) -lXt $(EXTENSIONLIB) -lX11 -lm
LIBS=/usr/proj/amwork/conf/amoeba/mc68000.sun/X/lib/Xaw/libXaw.a /usr/proj/amwork/conf/amoeba/mc68000.sun/X/lib/Xmu/libXmu.a /usr/proj/amwork/conf/amoeba/mc68000.sun/X/lib/Xt/libXt.a /usr/proj/amwork/conf/amoeba/mc68000.sun/X/extensions/lib/libXext.a /usr/proj/amwork/conf/amoeba/mc68000.sun/X/lib/X/libX11.a /usr/proj/amwork/conf/amoeba/mc68000.sun/lib/ajax/libajax.a /usr/proj/amwork/conf/amoeba/mc68000.sun/lib/amoeba.otcp/libamoeba.a /usr/proj/amwork/conf/amoeba/mc68000.sun/lib/math/libmath.a 
SRCS=xdvi.c dvi_init.c dvi_draw.c $(FONTFORMATS_C) pxl_open.c tpic.c
OBJS=xdvi.o dvi_init.o dvi_draw.o $(FONTFORMATS_O) pxl_open.o tpic.o
CC=cc
RM=rm -f
LDFLAGS=-Bstatic -e start
HEAD=/usr/proj/amwork/conf/amoeba/mc68000.sun/lib/head/head.o


.c.o:
	$(CC) -c $(CFLAGS) $*.c

xdvi:	$(OBJS)
	ld $(LDFLAGS) -o xdvi \
	$(HEAD) $(OBJS) $(LIBS)


xdvi.o: xdvi.h patchlevel.h xdvi.icon
dvi_init.o:xdvi.h dvi.h
dvi_draw.o:xdvi.h dvi.h
tpic.o:	xdvi.h

gf.o:	xdvi.h
pk.o:	xdvi.h
pxl.o:	xdvi.h

pxl_open.o:
	$(CC) -c $(CFLAGS) $(FONTDEFINES) pxl_open.c

xdvi.man: xdvi_man.sed
	chmod u+x mksedscript
	mksedscript $(DEFAULT_FONT_PATH) $(DEFAULT_FONT_SIZES) $(DEFINES) \
		> sedscript
	sed -f sedscript < xdvi_man.sed > xdvi.man

install: xdvi 
#xdvi.man
#	install -c -s -m 755 xdvi $(BINDIR)
#	install -c -m 644 xdvi.man $(MANDIR)/man1/xdvi.1
	ainstall xdvi /super/util/xdvi

clean:
	$(RM) *.o *.BAK *.CKP sedscript xdvi.man xdvi a.out core errs *~
	$(RM) xdvi10.man.s

lint:
	lint $(INCLUDES) $(DEFINES) $(FONTDEFINES) $(SRCS)
