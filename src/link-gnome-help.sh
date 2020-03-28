#!/bin/bash

set -e

gnome_help="${MESON_INSTALL_DESTDIR_PREFIX}/bin/gnome-help"

rm -f "${gnome_help}"
ln -s yelp "${gnome_help}"
