
prefix  = /usr/local
bindir  = $(prefix)/bin
man1dir = $(prefix)/share/man/man1
man1ext = .1

CC        = gcc
CFLAGS    = -Wall -Wextra -Wno-unused-parameter -g
ALLCFLAGS = $(CFLAGS) -std=gnu11
AS        = as
AR        = ar
ARFLAGS   = rcs
LD        = gcc
LDFLAGS   =

target = ufiber.a

libobjects = arch.o ufiber.o
objects = $(libobjects) test.o
clean = $(objects) $(target) test

all: $(target)

include rules.mk

$(target): $(libobjects)
	$(call cmd,ar)

test: $(objects) test.o
	$(call cmd,ld)
