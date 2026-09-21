#!/bin/sh
# Build and install BigNums locally, or into standard system search paths.
set -eu
case ${1-} in
    -h|--help)
        cat <<'USAGE'
Usage: ./build.sh [--system | NAME=value ...]
Build and install the shared library, headers, and Unix manual pages.
Default PREFIX: $HOME/.local (or the PREFIX environment variable).
--system installs into /usr/local, registers the loader path, and runs ldconfig.
It builds first, then uses sudo for installation unless already root.
System mode accepts no NAME=value overrides.
Examples:
  ./build.sh
  ./build.sh --system
  ./build.sh PREFIX=/usr/local
  ./build.sh PREFIX=/usr DESTDIR=/tmp/bignums-package
  ./build.sh CC=clang CFLAGS='-O2 -Wall -Wextra'
For a build without installation: make -C bigNums shared
Linux/ELF shared-library toolchain and GNU Make are required.
USAGE
        exit 0 ;;
esac
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "${1-}" = --system ]; then
    if [ "$#" -ne 1 ]; then
        printf '%s\n' '--system does not accept installation overrides.' >&2
        exit 2
    fi
    as_root() {
        if [ "$(id -u)" -eq 0 ]; then
            "$@"
        else
            sudo -- "$@"
        fi
    }
    if [ "$(id -u)" -ne 0 ] && ! command -v sudo >/dev/null 2>&1; then
        printf '%s\n' 'System installation requires sudo or running as root.' >&2
        exit 1
    fi
    loader_tool=$(command -v ldconfig || true)
    if [ -z "$loader_tool" ]; then
        for candidate in /sbin/ldconfig /usr/sbin/ldconfig; do
            if [ -x "$candidate" ]; then loader_tool=$candidate; break; fi
        done
    fi
    if [ -z "$loader_tool" ]; then
        printf '%s\n' 'System installation requires ldconfig (Linux/glibc).' >&2
        exit 1
    fi
    # Keep compilation unprivileged and use the same artifact path after sudo.
    make -C "$script_dir/bigNums" shared "BUILD_DIR=$script_dir/build"
    printf '%s\n' 'Installing into /usr/local and registering /usr/local/lib with the loader.'
    as_root make -C "$script_dir/bigNums" install-library \
        "BUILD_DIR=$script_dir/build" PREFIX=/usr/local LIBDIR=/usr/local/lib \
        INCLUDEDIR=/usr/local/include DESTDIR=
    as_root "$script_dir/man_setup.sh" PREFIX=/usr/local \
        MANDIR=/usr/local/share/man DESTDIR=
    loader_config=$(mktemp)
    trap 'rm -f "$loader_config"' EXIT HUP INT TERM
    printf '%s\n' '/usr/local/lib' > "$loader_config"
    as_root install -d /etc/ld.so.conf.d
    as_root install -m 644 "$loader_config" /etc/ld.so.conf.d/bignums.conf
    as_root "$loader_tool"
    printf '\n%s\n' 'Installed. Compile with: cc app.c -lbignums -o app' \
        'Read the manual with: man 7 bignums'
    exit 0
fi
for arg do
    case $arg in
        *=*) ;;
        *) printf 'Expected NAME=value, got: %s\n' "$arg" >&2; exit 2 ;;
    esac
done
install_prefix=${PREFIX:-"$HOME/.local"}
make -C "$script_dir/bigNums" install-library "PREFIX=$install_prefix" "$@"
"$script_dir/man_setup.sh" "PREFIX=$install_prefix" "$@"
make -s -C "$script_dir/bigNums" link-help "PREFIX=$install_prefix" "$@"
