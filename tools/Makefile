# Makefile for XRoar.
# You need to run 'configure' first.

# Run with "VERBOSE=1" for normal output.

.PHONY: all
all: build-bin

-include ../config.mak

SRCROOT ?= ..
include $(SRCROOT)/common.mak
SRCROOT := $(SRCROOT)/tools

############################################################################
# Tools

# font2c

font2c: $(SRCROOT)/font2c.c
	$(call do_build_cc,$@,$(opt_build_sdl_CFLAGS) $< $(opt_build_sdl_LDFLAGS) $(opt_build_sdl_image_LDFLAGS))

tools_CLEAN += font2c

.PHONY: build-bin
build-bin: font2c

############################################################################
# Clean-up, etc.

.PHONY: clean
clean:
	rm -f $(tools_CLEAN)

.PHONY: profile-clean
profile-clean:

.PHONY: distclean
distclean: clean profile-clean

.PHONY: depend
depend:
