#	
#	@(#)Makefile	1.1	91/11/21 11:41:13
#
#	MMDF Program Source Subtree Makefile
#

#
# Programs that live in subdirectories, and have makefiles of their own.
#
SUBDIR=	tools submit deliver local list delay smtp #uucp prog blockaddr #asmtp
#pop

#
#  This should list all the subdirectories that make clean works in.
#  This probably should not be altered unless you add/delete a channel.
#
ALLSUBDIRS=	tools submit deliver local list delay smtp phone pobox \
		uucp niftp bboards pop ean prog smphone blockaddr asmtp

all:
	for dir in ${SUBDIR}; do \
		(cd $${dir}; gen ${MFLAGS} -${MAKEFLAGS}); done

install:
	for dir in ${SUBDIR}; do \
		(cd $${dir}; gen ${MFLAGS} -${MAKEFLAGS} install); done

aminstall:
	for dir in ${SUBDIR}; do \
		(cd $${dir}; gen ${MFLAGS} -${MAKEFLAGS} aminstall); done

dist:
	-touch `ls */x*|grep -v '\.'`
	echo echo MMDF Distribution made `date` > install.sh
	for dir in ${SUBDIR}; do \
		echo cd $${dir} >> install.sh; \
		( cd $${dir}; \
		  gen -n ${MFLAGS} -${MAKEFLAGS} install) >> install.sh; \
		echo cd .. >> install.sh; \
	done
	tar cvf mmdfdist.tar install.sh */x*

depend:
	for dir in ${SUBDIR}; do \
		(cd $${dir}; gen ${MFLAGS} -${MAKEFLAGS} depend); done

lint:
	for dir in ${SUBDIR}; do \
		(cd $${dir}; gen ${MFLAGS} -${MAKEFLAGS} lint); done

clean:
	-rm -f make.out core
	for dir in ${ALLSUBDIRS}; do \
		(cd $${dir}; make -f Makefile.real clean); done
