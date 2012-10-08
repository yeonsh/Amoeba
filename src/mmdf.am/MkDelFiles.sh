#!/bin/sh
#	
#	@(#)MkDelFiles.sh	1.2	96/02/27 09:55:46
#
#
# MkDeleteFiles - Create a script on standard output that will
# delete all files from another hierarchy that are in this hierarchy.
# First generate a sanity check:
cat <<FOOBIE
#!/bin/sh
if [ ! -f DeleteFiles.sh ]; then
    echo OOPS! You are running DeleteFiles.sh in the wrong directory!
    exit 1
fi
FOOBIE
find . ! -type d -print | sed -e 's/^/rm -f /'
