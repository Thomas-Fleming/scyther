#!/bin/sh
#
# Make a new distribution archive. Command line specification of the tag
#
# For now, just windows. Others will follow.
#
# Usage will be:
#
#   gitdist ARCH TAG
#
#   ARCH is any of
#
#	linux
#	w32
#	mac
#
# The tag is checked out of the current repository (so it should exist)
# and this is used to construct a archive with the binary of the
# selected architecture.

if [ "x$1" = "x" -o "x$2" = "x" ]
then
	echo
	echo "Scyther binary distribution generator."
	echo
	echo "  Usage: $0 <arch> <tag>"
	echo
	echo "where <arch> is one of linux,w32,mac"
	echo "and <tag> is any tag in the current git repository."
	echo
	exit
fi

ARCH=$1
if [ "x$ARCH" = "xlinux" -o "x$ARCH" = "xw32" -o "x$ARCH" = "xmac" ]
then
	echo "Architecture $ARCH detected."
else
	echo "Don't know architecture $ARCH."
	echo "Please use one of linux,w32,mac"
	exit
fi

TAG=$2
FOUND=`git-tag -l $TAG`
if [ "x$TAG" = "x$FOUND" ]
then
	echo "Tag $TAG found."
else
	echo "Don't know tag $TAG, please select one from below:"
	git-tag -l
	exit
fi

# Note without extension, this will added later
ARCHNAME=scyther-$ARCH-$TAG

# Directory locations
CURDIR=`pwd`
DESTDIR=$CURDIR
TMPDIR="/tmp"
SRCNAME=$ARCHNAME-src

# Hard coded connections, do not change this (hardcoded in git-archive
# usage and archive creation)
SRCDIR=$TMPDIR/$SRCNAME		
BUILDDIR=$TMPDIR/$ARCHNAME

# Archive destination file without extension
DESTFILE=$DESTDIR/$ARCHNAME

# Internal locations
DOCDIR=$SRCDIR/doc/manual
MANUAL=scyther-manual.pdf

rm -rf $SRCDIR
rm -rf $BUILDDIR

# Change into the lower directory (main archive dir)
cd .. && git-archive --format=tar --prefix=$SRCNAME/ $TAG | (cd $TMPDIR && tar xf -)

# Base of the package is the gui directory
mv $SRCDIR/gui $BUILDDIR

# Prepare version.h with the correct flag (tag)
echo "#define SVNVERSION \"Unknown\"" >$SRCDIR/src/version.h
echo "#define TAGVERSION \"$TAG\"" >>$SRCDIR/src/version.h
echo "" >>$SRCDIR/src/version.h

# Manual
cp $DOCDIR/$MANUAL $BUILDDIR

# Change into sources directory
cd $SRCDIR/src

# Default flags
CMFLAGS="-D CMAKE_BUILD_TYPE:STRING=Release"
if [ $ARCH = "w32" ]
then
	BIN="scyther-w32.exe"
	cmake $CMFLAGS -D TARGETOS=Win32 . && make

elif [ $ARCH = "linux" ]
then
	BIN="scyther-linux"
	cmake $CMFLAGS . && make

elif [ $ARCH = "mac" ]
then
	# Make for ppc and intel, and combine into universal binary
	BIN="scyther-mac"
	cmake $CMFLAGS -D TARGETOS=MacPPC   . && make
	cmake $CMFLAGS -D TARGETOS=MacIntel . && make
	cmake $CMFLAGS                      . && make scyther-mac
fi

# Copy the resulting binary to the correct location
BINDIR=$BUILDDIR/Scyther/Bin
mkdir $BINDIR
cp $BIN $BINDIR

# Prepare tag for gui version
echo "SCYTHER_GUI_VERSION = \"$TAG\"" >$BUILDDIR/Gui/Version.py

# Compress the whole thing into an archive
cd $TMPDIR
if [ $ARCH = "w32" ]
then
	DESTARCH=$DESTFILE.zip
	rm -f $DESTARCH
	zip -r $DESTARCH $ARCHNAME
else
	DESTARCH=$DESTFILE.tgz
	rm -f $DESTARCH
	tar zcvf $DESTARCH $ARCHNAME
fi

# Remove the temporary working directory
rm -rf $BUILDDIR
rm -rf $SRCDIR


