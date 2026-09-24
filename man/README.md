# BigNums Unix manual


> Well...I am a man-pages guy. If you are too, take a peek through
> [demo.c](../bigNums/demo.c), then `man 3 the_function_you_intend_to_use`
> and go feral. Read the return values before the return values fuck with you.

Start with `bignums(7)` for building and linking, `bignums(3)` for the types,
and the function pages for precision, aliasing, and error behavior and shit

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

Install just the manual into your personal prefix:

```sh
./man_setup.sh
man -M "$HOME/.local/share/man" 7 bignums
```

`./build.sh` also calls `man_setup.sh` after installing the shared library and
headers. `./build.sh --system` installs the manuals under
`/usr/local/share/man` using sudo unless already root, alongside the library and loader setup.

Without that flag, both scripts default to `$HOME/.local`; direct Make installation
defaults to `/usr/local`. `MANDIR` defaults to `$(PREFIX)/share/man`.
`DESTDIR` supports package staging, for example:

```sh
make -C bigNums install-man DESTDIR=/tmp/bignums-package PREFIX=/usr
man -M /tmp/bignums-package/usr/share/man 3 bigFloatDiv
```
