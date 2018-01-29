#!/bin/bash

set -e

gnome_help="${DESTDIR}/${MESON_INSTALL_PREFIX}/bin/gnome-help"

rm -f "${gnome_help}"
ln -s yelp "${gnome_help}"
