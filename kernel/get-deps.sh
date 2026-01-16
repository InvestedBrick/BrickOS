#! /bin/sh

set -e

srcdir="$(dirname "$0")"
test -z "$srcdir" && srcdir=.

cd "$srcdir"

clone_repo_commit() {
    if test -d "$2/.git"; then
        git -C "$2" reset --hard
        git -C "$2" clean -fd
        if ! git -C "$2" -c advice.detachedHead=false checkout $3; then
            rm -rf "$2"
        fi
    else
        if test -d "$2"; then
            echo "error: '$2' is not a Git repository" 1>&2
            exit 1
        fi
    fi
    if ! test -d "$2"; then
        git clone $1 "$2"
        if ! git -C "$2" -c advice.detachedHead=false checkout $3; then
            rm -rf "$2"
            exit 1
        fi
    fi
}

fetch_uACPI() {

    git clone https://github.com/uACPI/uACPI.git acpi/uacpi

    cd acpi/uacpi || exit 0
    shopt -s dotglob nullglob
    for entry in *; do
        case "$entry" in
            source|include) continue ;;
            *) rm -rf -- "$entry" ;;
        esac
    done

    cd ../..

}

if [ -f .deps-obtained ]; then
    echo "Dependencies already obtained."
    exit 0
fi


clone_repo_commit \
    https://codeberg.org/OSDev/freestnd-c-hdrs-0bsd.git \
    freestnd-c-hdrs \
    5e4e9e70278fe89ea328d359a58aff4f4a94b165

clone_repo_commit \
    https://codeberg.org/OSDev/cc-runtime.git \
    cc-runtime \
    dae79833b57a01b9fd3e359ee31def69f5ae899b

clone_repo_commit \
    https://codeberg.org/Limine/limine-protocol.git \
    limine-protocol \
    fedf97facd1c473ee8720f8dfd5a71d03490d928

fetch_uACPI

touch .deps-obtained

printf "\nDependencies obtained successfully.\n"
