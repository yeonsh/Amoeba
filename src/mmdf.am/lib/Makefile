#	
#	@(#)Makefile	1.1	91/11/27 13:16:26
#
#
#	Makefile for the MMDF library   (libmmdf.a)
#

#  The paths below are relative from the directories below this one.
#
#  The RANLIB define should be "sh libfix" for systems that don't have
#	the ranlib command.  Systems that do have ranlib should set
#	RANLIB to "ranlib".  "libfix" is a Bourne shell file that will
#	use lorder and tsort to reorder the library.
#
MAKE	= make
RANLIB	= ranlib

SUBDIRS = addr mmdf table util
ALLSUBDIRS = addr dial mmdf table util

default: remake reorder

remake:
	for dir in $(SUBDIRS); \
	  do (cd $${dir}; echo "Running make in $${dir}"; \
	      ./gen ${MFLAGS} -${MAKEFLAGS}); \
	  done
#	cd ../libz; ./gen 

install: default
#	cd ../libz; ./gen install
	@echo "Library is up to date, nothing else to do."

aminstall:
	@echo "Nothing in mmdf library is to be installed.  Continuing...."

reorder: reorder-done

reorder-done: libmmdf.a
	-$(RANLIB) libmmdf.a
	-touch reorder-done

depend:
	for dir in $(SUBDIRS); \
	  do (cd $${dir}; ./gen ${MFLAGS} -${MAKEFLAGS} depend); done

lint:
	for dir in $(SUBDIRS); \
	  do (cd $${dir}; ./gen ${MFLAGS} -${MAKEFLAGS} lint); done
	cat */llib-l*.ln > llib-lmmdf.ln

clean:
	-rm -f *-made libmmdf.a make.out core llib-lmmdf.ln reorder-done
	for dir in $(SUBDIRS); \
	  do (cd $${dir}; $(MAKE) -f Makefile.real clean); done

ALWAYS:















