ufibers - user space fibers
===========================

Copyright © 2013-2015 Drew Thoreson


About
-----

ufibers is a user space fibers/coroutines library.  A fiber is much like a
thread, except that fibers are voluntarily preempted.  Because ufibers is
implemented completely in user space, it does not depend on any special
operating system support.


Building
--------

You'll need a recent version of gcc.  Currently ufibers supports x86 and
x86\_64.

To build the library as an archive:

    $ make ufiber.a

Then link the generated archive (ufibers.a) with your program.

To build the library as a shared object:

    $ make so

To install the shared library:

    $ make install

Then link your program with -lufiber.


Git Repository
--------------

https://github.com/drewt/ufibers

    $ git clone https://github.com/drewt/ufibers.git
