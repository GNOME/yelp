#!/bin/sh

set -e

itstool=""

parse_options() {
    while [[ -n "${1}" ]]; do
        case "${1}" in
            --itstool)
                itstool="${2}"
                shift 2
                ;;
            --help)
                cat <<EOF
Usage ${0} [FLAGS]
FLAGS:
--itstool <PATH> - path to the itstool program
--help - prints this message and quits
EOF
                ;;
            *=*)
                echo "--foo=bar flags are not supported, use --foo bar"
                exit 1
                ;;
            *)
                echo "unknown flag ${1}, use --help to get help" >&2
                exit 1
                ;;
        esac
    done

    if [ -z "${itstool}" ]; then
        echo "path to itstool program not specified" >&2
        exit 1
    fi
}

parse_options "${@}"

# Note that this modifies the file in source directory, not build
# directory, because the changes are supposed to be committed to the
# repository.
cd "${MESON_SOURCE_ROOT}/${MESON_SUBDIR}"
"${itstool}" -o 'yelp.pot' 'yelp.xml.in'
