# BigNums Unix manual


> Well...I am a man-pages guy so if you are one too don't bother with the DOCUMENTATION to implement shit 
take a peek through [demo.c](/bigNums/demo.c) and then `man 3 the_fucntion_you_intend_to_use` and go feral... 

From the repository root, read the manual directly:

```sh
man -M "$PWD/man" 7 bignums
man -M "$PWD/man" 3 bignums
man -M "$PWD/man" 3 BigInt
man -M "$PWD/man" 3 bigIntFactorial
man -M "$PWD/man" 3 bigFloatDiv
man -M "$PWD/man" 3 fft_arbitrary
```

Direct-file viewing also works from the repository root:

```sh
man -l man/man3/bigFloatDiv.3
man -l man/man7/bignums.7
```

The pages use roff `man` macros. Related functions share canonical pages;
relative symlinks provide the other names in the source tree. Installation
dereferences those links into complete roff files, so installed pages do not
depend on the viewer's working directory or a `.so` search path.

To render all pages with groff, including aliases:

```sh
make -C bigNums man
```

Rendered terminal text is written beneath `build/man/` (ignored by Git).
Override `GROFF` or `MAN_BUILD_DIR` on the Make command line if needed.
Render a single source directly:

```sh
groff -man -Tutf8 man/man3/bigFloatDiv.3
```

Check formatting and lookup with Python 3, man, groff, and col:

```sh
make -C bigNums check-man
```

This checks every page at 60, 80, and 100 columns using direct-file lookup,
source-tree lookup, and a temporary staged installation. It rejects formatter
diagnostics, missing sections, overlong lines, and incorrect rendered C
prototypes. Plain-text previews are saved under `build/man-review/` for reading.

Install just the manual into your personal prefix:

```sh
./man_setup.sh
man -M "$HOME/.local/share/man" 7 bignums
```

`./build.sh` also calls `man_setup.sh` after installing the shared library and
headers. `./build.sh --system` installs the manuals under
`/usr/local/share/man` using sudo alongside the library(again I expect user to inspect the sudo command usage...don't trust my words) and loader setup.

Without that flag, both scripts default to `$HOME/.local`; direct Make installation
defaults to `/usr/local`. `MANDIR` defaults to `$(PREFIX)/share/man`.
`DESTDIR` supports package staging, for example:

```sh
make -C bigNums install-man DESTDIR=/tmp/bignums-package PREFIX=/usr
man -M /tmp/bignums-package/usr/share/man 3 bigFloatDiv
```

When changing the API, edit the canonical page containing its synopsis and
behavior. Keep aliases as relative symlinks to the canonical file in the same
directory. Verify prototypes against `bignums.h` and `complexFFT.h` and run
`make -C bigNums check-man`. Keep examples and limitations consistent with the
library.
