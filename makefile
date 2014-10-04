.PHONY: so install uninstall

libmajor = 0
libminor = 1
libname  = libufiber.so
soname   = $(libname).$(libmajor)
realname = $(soname).$(libminor)

prefix  = /usr/local
bindir  = $(prefix)/bin
libdir  = $(prefix)/lib
mandir = $(prefix)/share/man

CC        = gcc
CFLAGS    = -Wall -Wextra -Wno-unused-parameter -g
ALLCFLAGS = $(CFLAGS) -std=gnu11
AS        = as
AR        = ar
ARFLAGS   = rcs
LD        = $(CC)
LDFLAGS   =
INSTALL   = @scripts/install

man3 = doc/ufiber_create.3 doc/ufiber_exit.3 doc/ufiber_join.3 \
       doc/ufiber_ref.3 doc/ufiber_self.3 doc/ufiber_yield.3

libobjects = arch.o ufiber.o
soobjects = $(addprefix so.,$(libobjects))
objects = $(libobjects) $(soobjects) check.o
clean = $(objects) $(realname) ufiber.a test

all: ufiber.a

so: $(realname)

include rules.mk

quiet_cmd_ldconf = LDCONF  $(libdir)
      cmd_ldconf = ldconfig -n $(libdir)

quiet_cmd_libln = LN      $(libdir)/$(libname)
      cmd_libln = ln -f -s $(realname) $(libdir)/$(libname)

quiet_cmd_sold = LD      $@
      cmd_sold = $(LD) $(LDFLAGS) -shared -Wl,-soname,$(1) -o $@ $^

so.%.o: %.c
	$(call cmd,cc,-fPIC)

so.%.o: %.S
	$(call cmd,ccas)

$(realname): $(soobjects)
	$(call cmd,sold,$(soname))

ufiber.a: $(libobjects)
	$(call cmd,ar)

check: check.o ufiber.a
	$(call cmd,ld,-lcheck)

install: $(realname)
	$(INSTALL) -m755 $(libdir) $(realname)
	$(call cmd,ldconf)
	$(call cmd,libln)
	$(INSTALL) -m644 $(mandir)/man3 $(man3)

uninstall:
	rm -f $(libdir)/$(realname)
	rm -f $(libdir)/$(soname)
	rm -f $(libdir)/$(libname)
