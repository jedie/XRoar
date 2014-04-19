# Makefile for XRoar.
# You need to run 'configure' first.

# Run with "VERBOSE=1" for normal output.

.PHONY: all
all: build-lib

-include ../config.mak

SRCROOT ?= ..
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
	c-ctype.c \
	c-strcasecmp.c \
	c-strncasecmp.c \
	delegate.c \
	slist.c \
	strsep.c \
	xmalloc.c
portalib_BASE_C_O = $(portalib_BASE_C:.c=.o)
#
portalib_SOURCES_C += $(portalib_BASE_C)
portalib_CLEAN_O += $(portalib_BASE_C_O)
portalib_C_O = $(portalib_BASE_C_O)

############################################################################
# Options

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

.PHONY: uninstall
uninstall:

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
