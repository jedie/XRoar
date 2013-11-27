# Common rules

#VERBOSE = 1
VERSION_MAJOR = 0
VERSION_MINOR = 30
VERSION_PATCH = 0
VERSION_SUBPATCH = 0

VERSION = $(VERSION_MAJOR).$(VERSION_MINOR)

# VERSION_SUBPATCH only non-zero for snapshot builds, in which case it is the
# month and day parts of the snapshot date, and VERSION_PATCH will be set to
# the year part.

ifneq ($(VERSION_PATCH),0)
ifeq ($(VERSION_SUBPATCH),0)
VERSION := $(VERSION).$(VERSION_PATCH)
else
VERSION := snap-$(VERSION_PATCH)-$(VERSION_SUBPATCH)
endif
endif

DISTNAME = xroar-$(VERSION)

.PHONY: profile-generate profile-use
profile-generate: CFLAGS += -fprofile-generate -ftest-coverage
profile-generate: CXXFLAGS += -fprofile-generate -ftest-coverage
profile-generate: OBJCFLAGS += -fprofile-generate -ftest-coverage
profile-generate: LDFLAGS += -fprofile-generate -ftest-coverage
profile-use: CFLAGS += -fprofile-use
profile-use: CXXFLAGS += -fprofile-use
profile-use: OBJCFLAGS += -fprofile-use
profile-use: LDFLAGS += -fprofile-use

############################################################################
# Base object files and settings required by all builds

ifeq ($(VERBOSE),)

do_cc = @echo CC $(1); $(CC) -o $(1) $(2)
do_cxx = @echo CXX $(1); $(CXX) -o $(1) $(2)
do_objc = @echo OBJC $(1); $(OBJC) -o $(1) $(2)
do_ar = @echo AR $(1); $(AR) cru $(1) $(2)
do_ranlib = @echo RANLIB $(1); $(RANLIB) $(1)
do_ld = @echo LD $(1); $(CC) -o $(1) $(2)
do_build_cc = @echo BUILD_CC $(1); $(BUILD_CC) -o $(1) $(2)
do_build_cxx = @echo BUILD_CXX $(1); $(BUILD_CXX) -o $(1) $(2)
do_build_objc = @echo BUILD_OBJC $(1); $(BUILD_OBJC) -o $(1) $(2)
do_windres = @echo WINDRES $(1); $(WINDRES) -o $(1) $(2)
do_makeinfo = @echo MAKEINFO $(1); $(MAKEINFO) -o $(1) $(2)
do_texi2pdf = @echo TEXI2PDF $(1); $(TEXI2PDF) -o $(1) $(2)

else

do_cc = $(CC) -o $(1) $(2)
do_cxx = $(CXX) -o $(1) $(2)
do_objc = $(OBJC) -o $(1) $(2)
do_ar = $(AR) cru $(1) $(2)
do_ranlib = $(RANLIB) $(1)
do_ld = $(CC) -o $(1) $(2)
do_build_cc = $(BUILD_CC) -o $(1) $(2)
do_build_cxx = $(BUILD_CXX) -o $(1) $(2)
do_build_objc = $(BUILD_OBJC) -o $(1) $(2)
do_windres = $(WINDRES) -o $(1) $(2)
do_makeinfo = $(MAKEINFO) -o $(1) $(2)
do_texi2pdf = $(TEXI2PDF) -o $(1) $(2)

endif

############################################################################
# Install rules

ifneq ($(INSTALL),)
INSTALL_FILE    = $(INSTALL) -m 0644
ifeq (,$(filter nostrip,$(DEB_BUILD_OPTIONS)))
INSTALL_PROGRAM = $(INSTALL) -m 0755 -s
else
INSTALL_PROGRAM = $(INSTALL) -m 0755
endif
INSTALL_SCRIPT  = $(INSTALL) -m 0755
INSTALL_DIR     = $(INSTALL) -d -m 0755
else
INSTALL_FILE    = cp
INSTALL_PROGRAM = cp -p
INSTALL_SCRIPT  = cp
INSTALL_DIR     = mkdir -p
endif
