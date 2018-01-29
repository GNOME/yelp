#!/bin/bash

set -e

po_dir=''
itstool=''
msgfmt=''
input=''
output=''
target=''

parse_options() {
    while [[ -n "${1}" ]]; do
        case "${1}" in
            --input)
                input="${2}"
                shift 2
                ;;
            --itstool)
                itstool="${2}"
                shift 2
                ;;
            --msgfmt)
                msgfmt="${2}"
                shift 2
                ;;
            --output)
                output="${2}"
                shift 2
                ;;
            --po-dir)
                po_dir="${2}"
                shift 2
                ;;
            --target)
                target="${2}"
                shift 2
                ;;
            --help)
                cat <<EOF
Usage ${0} [FLAGS]
FLAGS:
--input <PATH> - path to the input xml file
--itstool <PATH> - path to the itstool program
--msgfmt <PATH> - path to the msgfmt program
--output <PATH> - path to the output xml file
--po-dir <PATH> - path to "po" directory; should also contain LINGUAS file
--target <NAME> - name of the target rule, used for deducing the temporary directory
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

    if [ -z "${input}" ]; then
        echo "path to input file not specified" >&2
        exit 1
    fi
    if [ -z "${itstool}" ]; then
        echo "path to itstool program not specified" >&2
        exit 1
    fi
    if [ -z "${msgfmt}" ]; then
        echo "path to msgfmt program not specified" >&2
        exit 1
    fi
    if [ -z "${output}" ]; then
        echo "path to output directory not specified" >&2
        exit 1
    fi
    if [ -z "${po_dir}" ]; then
        echo "path to po directory not specified" >&2
        exit 1
    fi
    if [ -z "${target}" ]; then
        echo "target name not specified" >&2
        exit 1
    fi
}

parse_options "${@}"

dir=$(dirname "${output}")
target_dir="${dir}/${target}.dir"
mkdir -p "${target_dir}"
all_linguas=$(grep -v '^#' "${po_dir}/LINGUAS" | tr '\n' ' ')
for lang in ${all_linguas}; do
    "${msgfmt}" -o "${target_dir}/${lang}.mo" "${po_dir}/${lang}.po"
done
"${itstool}" -o "${output}" -j "${input}" "${target_dir}"/*.mo
rm -rf "${target_dir}"
