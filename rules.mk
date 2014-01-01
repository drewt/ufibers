#
# rules.mk: build rules
#
#  For automatic dependency resolution, set $(objects) to the list of object
#  files needed for the build before including this file.
#
#  $(clean) and $(distclean) should be set to the files to be removed on
#  'make clean' and 'make distclean', respectively.
#
#  $(verbose) controls whether build commands are echoed verbatim, or in
#  prettier "  PROG    FILE" format.  Set verbose=y for verbatim output.
#

.SUFFIXES:
.PHONY: clean distclean

dependencies = $(addprefix ., $(addsuffix .d, $(basename $(objects))))
distclean += $(dependencies)

ifeq ($(verbose),y)
  quiet =
else
  quiet = quiet_
endif

clean:
	$(if $(clean), rm -f $(clean))

distclean: clean
	$(if $(distclean), rm -f $(distclean))

%.o: %.c
	$(call cmd,cc)

%.o: %.S
	$(call cmd,ccas)

.%.d: %.c
	$(call cmd,dep)

# CC for program object files (.o)
quiet_cmd_cc    = CC      $@
      cmd_cc    = $(CC) -c $(CPPFLAGS) $(ALLCFLAGS) $(1) -o $@ $<

# create archive
quiet_cmd_ar    = AR      $@
      cmd_ar    = $(AR) $(ARFLAGS) $@ $^

quiet_cmd_ccas  = AS      $@
      cmd_ccas  = $(CC) -c $(CPPFLAGS) $(1) -o $@ $<

quiet_cmd_as    = AS      $@
      cmd_as    = $(AS) $(ASFLAGS) -o $@ $<

# LD for programs; optional parameter: libraries
quiet_cmd_ld    = LD      $@
      cmd_ld    = $(LD) $(LDFLAGS) -o $@ $^ $(1)

# generate dependencies file
quiet_cmd_dep   = DEP     $@
      cmd_dep   = echo "$@ `$(CC) -MM $(CPPFLAGS) $<`" > $@

# call submake
quiet_cmd_smake = MAKE    $@
      cmd_smake = cd $@ && $(MAKE)

# generate man page from .txt file
quiet_cmd_ttman = MAN     $@
      cmd_ttman = doc/ttman $< $@

# generate man page as plain text file
quiet_cmd_groff = GROFF   $@
      cmd_groff = groff -man -Tascii $< | col -bx > $@

# cmd macro (taken from kbuild)
cmd = @$(if $($(quiet)cmd_$(1)),echo '  $(call $(quiet)cmd_$(1),$(2))' &&) $(call cmd_$(1),$(2))

ifneq ($(dependencies),)
-include $(dependencies)
endif
