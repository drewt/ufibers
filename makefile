.PHONY: so install uninstall

libmajor = 0
libminor = 1
libname  = libufiber.so
soname   = $(libname).$(libmajor)
realname = $(soname).$(libminor)

prefix  = /usr/local
bindir  = $(prefix)/bin
libdir  = $(prefix)/lib
man1dir = $(prefix)/share/man/man1
man1ext = .1

CC        = gcc
CFLAGS    = -Wall -Wextra -Wno-unused-parameter -g
ALLCFLAGS = $(CFLAGS) -std=gnu11
AS        = as
AR        = ar
ARFLAGS   = rcs
LD        = $(CC)
LDFLAGS   =
INSTALL   = install

libobjects = arch.o ufiber.o
soobjects = $(addprefix so.,$(libobjects))
objects = $(libobjects) $(soobjects) test.o
clean = $(objects) $(realname) ufiber.a test

all: ufiber.a

so: $(realname)

include rules.mk

cmd_install = @$(if $(quiet),echo '  INSTALL $(2)' &&) $(INSTALL) $(3) $(1) $(2)
cmd_ldconf  = @$(if $(quiet),echo '  LDCONF  $(2)' &&) ln -s $(1) $(2)

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

test: $(libobjects) test.o
	$(call cmd,ld)

install: $(realname)
	$(call cmd_install,$(realname),$(libdir)/$(realname))
	$(call cmd_ldconf,$(realname),$(libdir)/$(soname))
	$(call cmd_ldconf,$(realname),$(libdir)/$(libname))

uninstall:
	rm -f $(libdir)/$(realname)
	rm -f $(libdir)/$(soname)
	rm -f $(libdir)/$(libname)
