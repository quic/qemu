# Overview 

This document describes the Qualcomm® Hexagon™ QEMU-VP utility, which
is an emulator that decodes and translates the instructions of a DSP
program into instructions for a host architecture on a virtual
platform.

QEMU is designed differently from the Hexagon simulator. Instead of
simulating processor cycles, the instructions of a DSP program are
decoded and dynamically translated into instructions for the host
architecture. The execution will accurately emulate the architectural
behavior of the Hexagon DSP. Instructions are translated
just-in-time---as they are encountered during execution. This
translation cost is only paid once; executing the same code does not
require additional translation. Thus, the translation gives QEMU a
performance advantage over the simulator.

With QEMU, the default relationship between guest instruction
execution and guest system clocks is different from the simulator or
the target. Some DSP instructions require more host instructions than
others, which can cause some differences in how those clocks appear to
elapse during guest code emulation.

The software on the emulated DSP (the guest) can access the host
system via a set of semi- hosted *angel calls*, which are system calls
that the emulator handles to access the file system or console. The
QEMU interface matches the Hexagon simulator interface.

## Host system requirements

Releases of QEMU are provided for Linux. Starting with QEMU Hexagon
10.0, the Linux binaries will work on Ubuntu 20.

:::{note} An experimental release is also provided for Windows 
x86-64. This is a work-in-progress version with no semi-hosting or 
coprocessor support. The following dynamic libraries (DLLs) will also 
need to be installed: glib-2, libpixman-1, and libwinpthread-1. These 
libraries and their dependencies can be downloaded from:

- https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-glib2-2.74.6-1-any.pkg.tar.zst
- https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-pixman-0.40.0-2-any.pkg.tar.zst
- https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-gettext-0.21.1-2-any.pkg.tar.zst
- https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-pcre2-10.39-1-any.pkg.tar.zst
- https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-libiconv-1.17-3-any.pkg.tar.zst

::: 

## Coprocessor plugin

Some Hexagon DSP configurations utilize a separate coprocessor plugin.
The coprocessor plugin provides emulation support for additional
instructions that are not a part of the Hexagon core. The
QEMU_HEXAGON_COPROC environment variable must be set to the location
of this binary plugin. In Linux, this can be done with

:::{code-block}
export QEMU_HEXAGON_COPROC=<path to coprocessor plugin>
:::

## Required platform libraries

QEMU binaries distributed with the Hexagon Tools require the following
system libraries to be installed:

-   libasound
-   libc
-   libepoxy
-   libgcc_s
-   libgio
-   libglib
-   libgmodule
-   libgobject
-   libm
-   libpixman
-   libpthread
-   libpulse
-   libutil
-   libz
