# Makefile for XRoar.
# You need to run 'configure' first.

# Run with "VERBOSE=1" for normal output.

.PHONY: all
all: build-doc

-include ../config.mak

SRCROOT ?= $(dir $(lastword $(MAKEFILE_LIST)))
include $(SRCROOT)/common.mak
SRCROOT := $(SRCROOT)/doc

CONFIG_FILES = ../config.h ../config.mak

############################################################################
# Documentation

doc_CLEAN =

version.texi: $(CONFIG_FILES)
	echo "@set VERSION $(VERSION)" > $@

%.info: $(SRCROOT)/%.texi version.texi
	$(call do_makeinfo,$@,$<)

%.pdf: $(SRCROOT)/%.texi version.texi
	$(call do_texi2pdf,$@,-q --build=clean $<)

%.html: $(SRCROOT)/%.texi version.texi
	$(call do_makeinfo,$@,--html --no-headers --no-split $<)

%.txt: $(SRCROOT)/%.texi version.texi
	$(call do_makeinfo,$@,--plaintext --no-headers --no-split $<)

doc_CLEAN += xroar.info xroar.pdf xroar.html xroar.txt

.PHONY: build-doc
build-doc:

ifneq ($(MAKEINFO),)
build-doc: xroar.info
endif

############################################################################
# Install

.PHONY: install
install: install-doc

.PHONY: uninstall
uninstall: uninstall-doc

.PHONY: install-doc
install-doc: build-doc

.PHONY: uninstall-doc
uninstall-doc: uninstall-info

ifneq ($(MAKEINFO),)
install-doc: install-info
endif

.PHONY: install-info
install-info: xroar.info
	$(INSTALL_DIR) $(DESTDIR)$(infodir)
	$(INSTALL_FILE) xroar.info $(DESTDIR)$(infodir)

.PHONY: uninstall-info
uninstall-info:
	rm -f $(DESTDIR)$(infodir)/xroar.info
	-rmdir $(DESTDIR)$(infodir)

############################################################################
# Clean-up, etc.

.PHONY: clean
clean:
	rm -f $(doc_CLEAN)

.PHONY: profile-clean
profile-clean:

.PHONY: distclean
distclean: clean profile-clean

.PHONY: depend
depend:
