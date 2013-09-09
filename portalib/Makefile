# Makefile for XRoar.
# You need to run 'configure' first.

# Run with "VERBOSE=1" for normal output.

.PHONY: all
all: build-lib

-include ../config.mak

SRCROOT ?= $(dir $(lastword $(MAKEFILE_LIST)))
include $(SRCROOT)/common.mak
SRCROOT := $(SRCROOT)/portalib

CONFIG_FILES = ../config.h ../config.mak

############################################################################
# Base files

### Portalib

portalib_CLEAN_O =
portalib_CLEAN =
portalib_SOURCES_C =
portalib_C_O =

portalib_CFLAGS = $(CFLAGS) $(CPPFLAGS) \
	-I.. -I$(CURDIR) -I$(SRCROOT)/../portalib \
	$(WARN)

portalib_BASE_C = \
	strsep.c
portalib_BASE_C_O = $(portalib_BASE_C:.c=.o)
#
portalib_SOURCES_C += $(portalib_BASE_C)
portalib_CLEAN_O += $(portalib_BASE_C_O)
portalib_C_O = $(portalib_BASE_C_O)

############################################################################
# Options

portalib_glib2_C = \
	pl_glib/gmem.c \
	pl_glib/gslist.c \
	pl_glib/gstrfuncs.c
portalib_glib2_C_O = $(portalib_glib2_C:.c=.o)
#
portalib_SOURCES_C += $(portalib_glib2_C)
portalib_CLEAN_O += $(portalib_glib2_C_O)
ifneq ($(opt_glib2),yes)
	portalib_C_O += $(portalib_glib2_C_O)
endif

############################################################################
# Build

### Portalib

portalib_CLEAN += $(portalib_CLEAN_O)

portalib_OBJS = \
	$(portalib_C_O)

$(portalib_OBJS): $(CONFIG_FILES)

$(portalib_C_O): %.o: $(SRCROOT)/%.c
	$(call do_cc,$@,$(portalib_CFLAGS) -c $<)

libporta.a: $(portalib_OBJS)
	$(call do_ar,$@,$(portalib_OBJS))
	$(call do_ranlib,$@)

portalib_CLEAN += libporta.a

.PHONY: build-lib
build-lib: libporta.a

############################################################################
# Install

.PHONY: install
install:

############################################################################
# Clean-up, etc.

.PHONY: clean
clean:
	rm -f $(portalib_CLEAN)

.PHONY: profile-clean
profile-clean:
	rm -f $(portalib_CLEAN_O:.o=.gcda)
	rm -f $(portalib_CLEAN_O:.o=.gcno)

.PHONY: distclean
distclean: clean profile-clean

.PHONY: depend
depend:
	@touch .deps.mak
	makedepend -f .deps.mak -Y $(portalib_SOURCES_C) 2> /dev/null

-include .deps.mak
