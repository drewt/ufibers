ufibers - user space fibers
===========================

Copyright © 2013-2015 Drew Thoreson


About
-----

ufibers is a user space fibers/coroutines library.  A fiber is much like a
thread, except that fibers are voluntarily preempted.


Portability
-----------

ufibers is written in ISO C99, with a few assembly language routines to manage
machine contexts.  It does not depend on any special operating system support
and uses only a few functions from libc (malloc, free and exit), so it should
work on any operating system, or even on bare metal.

Since ufibers is written partially in assembly language, it is not portable
between architectures.  However, the asembly language routines are small and
self-contained, so it should be fairly easy to port for someone with working
knowledge of the target architecture.  Currently ufibers supports the x86 and
x86\_64 architectures.


Building
--------

You'll need a C compiler supporting the C99 standard.  GCC and Clang work.

To build the library as an archive:

    $ make ufiber.a

Then copy ufiber.h to your include path, `#include "ufiber.h"` in your source
files, and link your program against the generated archive (ufiber.a).

To build the library as a shared object:

    $ make so

To install the shared library:

    $ make install

Then `#include <ufiber.h>` in your source files and link your program with
-lufiber.


Git Repository
--------------

https://github.com/drewt/ufibers

    $ git clone https://github.com/drewt/ufibers.git
