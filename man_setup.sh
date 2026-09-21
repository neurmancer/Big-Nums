#!/bin/sh
# Keep the source manuals in the repository; copy them into the install tree.
set -eu
case ${1-} in
    -h|--help)
        cat <<'USAGE'
Usage: ./man_setup.sh [NAME=value ...]
Install the section 3 and 7 roff manuals without building the library.
Default PREFIX: $HOME/.local (or the PREFIX environment variable).
MANDIR defaults to PREFIX/share/man; DESTDIR supports package staging.
No sudo, formatter, or system manual-index update is invoked automatically.
USAGE
        exit 0 ;;
esac
for arg do
    case $arg in
        *=*) ;;
        *) printf 'Expected NAME=value, got: %s\n' "$arg" >&2; exit 2 ;;
    esac
done
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
install_prefix=${PREFIX:-"$HOME/.local"}
make -C "$script_dir/bigNums" install-man "PREFIX=$install_prefix" "$@"
