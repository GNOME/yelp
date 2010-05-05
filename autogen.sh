#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME=Yelp

if ! test -f $srcdir/src/yelp.c; then
  echo "**Error**: Directory '$srcdir' does not look like the yelp source directory"
  exit 1
fi

which gnome-autogen.sh || {
    echo "You need to install gnome-common package"
    exit 1
}

USE_GNOME2_MACROS=1 . gnome-autogen.sh
