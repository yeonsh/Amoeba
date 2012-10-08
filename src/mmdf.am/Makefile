#	
#	@(#)Makefile	1.1	91/11/21 11:51:03
#
#	MMDF Root Makefile
#
#
SUBDIR=	lib src uip

all:
	for dir in ${SUBDIR}; do \
		(cd $${dir}; make ${MFLAGS} -${MAKEFLAGS}); done

depend:
	for dir in ${SUBDIR}; do \
		(cd $${dir}; make ${MFLAGS} -${MAKEFLAGS} depend); done

lint:
	for dir in ${SUBDIR}; do \
		(cd $${dir}; make ${MFLAGS} -${MAKEFLAGS} lint); done

install:
	for dir in ${SUBDIR}; do \
		(cd $${dir}; make ${MFLAGS} -${MAKEFLAGS} install); done

aminstall:
	for dir in ${SUBDIR}; do \
		(cd $${dir}; make ${MFLAGS} -${MAKEFLAGS} aminstall); done

clean:
	-rm -f make.out
	for dir in ${SUBDIR}; do \
		(cd $${dir}; make clean); done
