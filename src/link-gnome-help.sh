#!/bin/bash

set -e

gnome_help="${MESON_INSTALL_DESTDIR_PREFIX}/bin/gnome-help"

ln -f -s yelp "${gnome_help}"
