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

Releases of QEMU are provided for Linux and Windows 10 or 11. Starting with
QEMU Hexagon 10.0, the Linux binaries will work on Ubuntu 20.

### Windows dependencies

1. First download and install msys2: [https://www.msys2.org/](https://www.msys2.org/)
2. On a msys2 shell, install the dependencies:
```
pacman -S mingw-w64-x86_64-libwinpthread-git mingw-w64-x86_64-glib2 \
          mingw-w64-x86_64-pixman mingw-w64-x86_64-libpng \
          mingw-w64-x86_64-gettext mingw-w64-x86_64-pcre2 \
          mingw-w64-x86_64-libiconv
```
3. Set up the Windows PATH variable to include the mingw directory when looking for DLLs:
    1. Open the Windows menu and search for "system variables", then click on
       "Edit the system environment variables".
    2. Click on the "Environment Variables" button at the bottom.
    3. At the bottom section, look for the "PATH" entry. Select it and hit "edit"
    4. Add two new entries:
    ```
    C:\msys64\mingw64\bin
    C:\msys64\mingw64\lib
    ```
    And hit "OK" in all the windows to save.

You should now be able to open CMD or PowerShell and run
`qemu-system-hexagon --help`.

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
