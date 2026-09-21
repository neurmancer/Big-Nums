# Big-Nums

An experimental C library for unsigned big integers and signed binary floats,
with FFT multiplication and Newton reciprocal/square-root routines. Numbers use
fixed storage: 128 32-bit limbs (4096 integer or mantissa bits).

Build and install the shared library, headers, and man pages on Linux:

```sh
./build.sh
```

For the usual system-wide `-lbignums` experience, run once:

```sh
./build.sh --system
cc app.c -lbignums -o app
man 7 bignums
```

System mode builds first, then requests sudo for installation into `/usr/local`.
It installs the headers and manuals in standard locations, registers
`/usr/local/lib` in `/etc/ld.so.conf.d/bignums.conf`, and runs `ldconfig`.
Use `#include <bignums.h>` in your application. No shell path edits or extra
compiler path flags are needed with the standard native Linux toolchain.
This mode requires a Linux/glibc system with `ldconfig` and accepts no path
overrides; use the prefix-based mode below for custom or staged installs.

Without `--system`, this installs into `$HOME/.local` without sudo. The script calls
`man_setup.sh` to copy the manuals into `share/man/man3` and `share/man/man7`.
Link your application against the installed library:

```sh
cc app.c -I"$HOME/.local/include" -L"$HOME/.local/lib" \
    -Wl,-rpath,"$HOME/.local/lib" -lbignums -o app
man -M "$HOME/.local/share/man" 7 bignums
```

Use `#include <bignums.h>` in `app.c`. The rpath tells the runtime loader where
to find the shared library; `-L` only controls the linker's search path.
The build requires a Linux/ELF C99 toolchain and GNU Make. No groff is needed
to install the manual sources.

For a system prefix, run `./build.sh PREFIX=/usr/local` with write access to
that prefix. Prefix-based installs do not invoke sudo or refresh the loader
cache; use `--system` for the complete standard-path setup. `DESTDIR` supports package staging:
`./build.sh PREFIX=/usr DESTDIR=/tmp/bignums-package`.
`LIBDIR`, `INCLUDEDIR`, and `MANDIR` can also be overridden.

Build without installing, run tests against the shared library, and build the demo:

```sh
make -C bigNums shared
make -C bigNums test demo
./build/demo
```

The existing custom-driver commands remain supported:

```sh
make -C bigNums MAIN=demo.c TARGET=demo
./bigNums/demo
make -C bigNums MAIN=test.c TARGET=test_big
./bigNums/test_big
```

Requires a C99-or-later compiler, Make, and the math library (`-lm`).
The numerical test suite checks arithmetic results and exits nonzero on failure.
Precision and decimal formatting still have limitations; this library remains
under construction.

See the [API documentation and known limitations](bigNums/DOCUMENTATION.md),
[usage demo](bigNums/demo.c), and [numerical tests](bigNums/test.c).

Unix manual pages are in [man/](man/README.md). Read them without installing:

```sh
man -M "$PWD/man" 7 bignums
man -M "$PWD/man" 3 bigIntFactorial
```

Well...this shit started as a side-quest from this repo and here we're...
[the library's origin](https://github.com/neurmancer/Basic-C-Examples/tree/main/reallyBasicThings/projects101/mathFuckery/DSPFuckery/bigNums).
