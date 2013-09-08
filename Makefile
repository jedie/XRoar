# Makefile for XRoar.
# You need to run 'configure' first.

# Run with "VERBOSE=1" for normal output.

-include config.mak

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
VERSION := snap-$(VERSION_PATCH)$(VERSION_SUBPATCH)
endif
endif

DISTNAME = xroar-$(VERSION)

.PHONY: all
all: build-lib build-bin build-doc

.PHONY: build-lib
.PHONY: build-bin
.PHONY: build-doc

.PHONY: profile-generate profile-use
profile-generate: CFLAGS += -fprofile-generate -ftest-coverage
profile-generate: CXXFLAGS += -fprofile-generate -ftest-coverage
profile-generate: OBJCFLAGS += -fprofile-generate -ftest-coverage
profile-generate: LDFLAGS += -fprofile-generate -ftest-coverage
profile-use: CFLAGS += -fprofile-use
profile-use: CXXFLAGS += -fprofile-use
profile-use: OBJCFLAGS += -fprofile-use
profile-use: LDFLAGS += -fprofile-use
profile-generate profile-use: all

SRCROOT ?= $(dir $(lastword $(MAKEFILE_LIST)))

############################################################################
# Base object files and settings required by all builds

ifeq ($(VERBOSE),)

WARN = -Wall -W

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

WARN = -Wall -W -Wstrict-prototypes -Wpointer-arith -Wcast-align \
	-Wcast-qual -Wshadow -Waggregate-return -Wnested-externs -Winline \
	-Wwrite-strings -Wundef -Wmissing-prototypes -Wredundant-decls

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

CONFIG_FILES = config.h config.mak

### Portalib

portalib_CLEAN =
portalib_SOURCES_C =
portalib_C_O =

portalib_CFLAGS = $(CFLAGS) $(CPPFLAGS) \
	-I$(CURDIR) -I$(SRCROOT)/portalib \
	$(WARN)

portalib_BASE_C = \
	portalib/strsep.c
portalib_BASE_C_O = $(portalib_BASE_C:.c=.o)
#
portalib_SOURCES_C += $(portalib_BASE_C)
portalib_CLEAN += $(portalib_BASE_C_O)
portalib_C_O = $(portalib_BASE_C_O)

### XRoar

xroar_CLEAN =
xroar_SOURCES_C =
xroar_SOURCES_CXX =
xroar_SOURCES_OBJC =
xroar_SOURCES_RC =
xroar_C_O =
xroar_CXX_O =
xroar_OBJC_O =
xroar_RC_RES =

xroar_CFLAGS = $(CFLAGS) $(CPPFLAGS) \
	-I$(CURDIR) -I$(SRCROOT)/portalib -I$(SRCROOT)/src \
	$(WARN) \
        -DVERSION=\"$(VERSION)\" \
        -DROMPATH=$(ROMPATH) -DCONFPATH=$(CONFPATH)
xroar_CXXFLAGS = $(CXXFLAGS) $(CPPFLAGS) \
	-I$(CURDIR) -I$(SRCROOT)/portalib -I$(SRCROOT)/src \
	$(WARN) \
        -DVERSION=\"$(VERSION)\" \
        -DROMPATH=$(ROMPATH) -DCONFPATH=$(CONFPATH)
xroar_OBJCFLAGS = $(OBJCFLAGS) $(CPPFLAGS) \
	-I$(CURDIR) -I$(SRCROOT)/portalib -I$(SRCROOT)/src \
	$(WARN) \
        -DVERSION=\"$(VERSION)\" \
        -DROMPATH=$(ROMPATH) -DCONFPATH=$(CONFPATH)
xroar_LDFLAGS = $(LDFLAGS) $(LDLIBS)

xroar_BASE_C = \
	src/becker.c \
	src/breakpoint.c \
	src/cart.c \
	src/crc16.c \
	src/crc32.c \
	src/crclist.c \
	src/deltados.c \
	src/dragondos.c \
	src/events.c \
	src/fs.c \
	src/hd6309.c \
	src/hexs19.c \
	src/joystick.c \
	src/keyboard.c \
	src/logging.c \
	src/machine.c \
	src/mc6809.c \
	src/mc6821.c \
	src/module.c \
	src/orch90.c \
	src/path.c \
	src/printer.c \
	src/romlist.c \
	src/rsdos.c \
	src/sam.c \
	src/snapshot.c \
	src/sound.c \
	src/tape.c \
	src/tape_cas.c \
	src/ui_null.c \
	src/vdg.c \
	src/vdg_bitmaps.c \
	src/vdg_palette.c \
	src/vdisk.c \
	src/vdrive.c \
	src/vo_null.c \
	src/wd279x.c \
	src/xconfig.c \
	src/xroar.c
xroar_BASE_C_O = $(xroar_BASE_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_BASE_C)
xroar_CLEAN += $(xroar_BASE_C_O)
xroar_C_O += $(xroar_BASE_C_O)

# Objects for all Unix-style builds (now the only one):
xroar_unix_C = src/main_unix.c
xroar_unix_C_O = $(xroar_unix_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_unix_C)
xroar_CLEAN += $(xroar_unix_C_O)
xroar_C_O += $(xroar_unix_C_O)

############################################################################
# Optional extras (most only apply to Unix-style build)

ifeq ($(opt_zlib),yes)
	xroar_CFLAGS += $(opt_zlib_CFLAGS)
	xroar_LDFLAGS += $(opt_zlib_LDFLAGS)
endif

ifeq ($(opt_glib2),yes)
	xroar_CFLAGS += $(opt_glib2_CFLAGS)
	xroar_LDFLAGS += $(opt_glib2_LDFLAGS)
endif

portalib_glib2_C = \
	portalib/pl_glib/gmem.c \
	portalib/pl_glib/gslist.c \
	portalib/pl_glib/gstrfuncs.c
portalib_glib2_C_O = $(portalib_glib2_C:.c=.o)
#
portalib_SOURCES_C += $(portalib_glib2_C)
portalib_CLEAN += $(portalib_glib2_C_O)
ifneq ($(opt_glib2),yes)
	portalib_C_O += $(portalib_glib2_C_O)
endif

xroar_gdb_C = src/gdb.c
xroar_gdb_C_O = $(xroar_gdb_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_gdb_C)
xroar_CLEAN += $(xroar_gdb_C_O)
ifeq ($(opt_gdb_target),yes)
	xroar_C_O += $(xroar_gdb_C_O)
endif

ifeq ($(opt_glib2),yes)
	xroar_CFLAGS += $(opt_glib2_CFLAGS)
	xroar_OBJCFLAGS += $(opt_glib2_CFLAGS)
	xroar_LDFLAGS += $(opt_glib2_LDFLAGS)
endif

xroar_gtk2_C = \
	src/gtk2/drivecontrol.c \
	src/gtk2/filereq_gtk2.c \
	src/gtk2/keyboard_gtk2.c \
	src/gtk2/tapecontrol.c \
	src/gtk2/ui_gtk2.c
xroar_gtk2_C_O = $(xroar_gtk2_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_gtk2_C)
xroar_CLEAN += $(xroar_gtk2_C_O)
ifeq ($(opt_gtk2),yes)
	xroar_C_O += $(xroar_gtk2_C_O)
	xroar_CFLAGS += $(opt_gtk2_CFLAGS)
	xroar_LDFLAGS += $(opt_gtk2_LDFLAGS)
src/gtk2/drivecontrol.o: src/gtk2/drivecontrol_glade.h
src/gtk2/tapecontrol.o: src/gtk2/tapecontrol_glade.h
src/gtk2/ui_gtk2.o: src/gtk2/top_window_glade.h
src/keyboard_gtk2.o: $(SRCROOT)/src/gtk2/keyboard_gtk2_mappings.c
endif

xroar_gtkgl_C = src/gtk2/vo_gtkgl.c
xroar_gtkgl_C_O = $(xroar_gtkgl_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_gtkgl_C)
xroar_CLEAN += $(xroar_gtkgl_C_O)
ifeq ($(opt_gtkgl),yes)
	xroar_C_O += $(xroar_gtkgl_C_O)
	xroar_CFLAGS += $(opt_gtkgl_CFLAGS)
	xroar_LDFLAGS += $(opt_gtkgl_LDFLAGS)
endif

xroar_opengl_C = src/vo_opengl.c
xroar_opengl_C_O = $(xroar_opengl_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_opengl_C)
xroar_CLEAN += $(xroar_opengl_C_O)
ifeq ($(opt_opengl),yes)
	xroar_C_O += $(xroar_opengl_C_O)
	xroar_CFLAGS += $(opt_opengl_CFLAGS)
	xroar_LDFLAGS += $(opt_opengl_LDFLAGS)
endif

xroar_sdl_C = \
	src/sdl/ao_sdl.c \
	src/sdl/common.c \
	src/sdl/joystick_sdl.c \
	src/sdl/keyboard_sdl.c \
	src/sdl/ui_sdl.c \
	src/sdl/vo_sdl.c \
	src/sdl/vo_sdlyuv.c
xroar_sdl_C_O = $(xroar_sdl_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_sdl_C)
xroar_CLEAN += $(xroar_sdl_C_O)
ifeq ($(opt_sdl),yes)
	xroar_C_O += $(xroar_sdl_C_O)
	xroar_CFLAGS += $(opt_sdl_CFLAGS)
	xroar_OBJCFLAGS += $(opt_sdl_OBJCFLAGS)
	xroar_LDFLAGS += $(opt_sdl_LDFLAGS)
src/sdl/keyboard_sdl.o: $(SRCROOT)/src/sdl/keyboard_sdl_mappings.c
endif

xroar_sdlgl_C = src/sdl/vo_sdlgl.c
xroar_sdlgl_C_O = $(xroar_sdlgl_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_sdlgl_C)
xroar_CLEAN += $(xroar_sdlgl_C_O)
ifeq ($(opt_sdlgl),yes)
	xroar_C_O += $(xroar_sdlgl_C_O)
	xroar_CFLAGS += $(opt_sdlgl_CFLAGS)
	xroar_LDFLAGS += $(opt_sdlgl_LDFLAGS)
endif

xroar_cli_C = src/filereq_cli.c
xroar_cli_C_O = $(xroar_cli_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_cli_C)
xroar_CLEAN += $(xroar_cli_C_O)
ifeq ($(opt_cli),yes)
	xroar_C_O += $(xroar_cli_C_O)
	xroar_CFLAGS += $(opt_cli_CFLAGS)
	xroar_LDFLAGS += $(opt_cli_LDFLAGS)
endif

xroar_cocoa_OBJC = \
	src/macosx/filereq_cocoa.m \
	src/macosx/ui_macosx.m
xroar_cocoa_OBJC_O = $(xroar_cocoa_OBJC:.m=.o)
#
xroar_SOURCES_OBJC += $(xroar_cocoa_OBJC)
xroar_CLEAN += $(xroar_cocoa_OBJC_O)
ifeq ($(opt_cocoa),yes)
	xroar_OBJC_O += $(xroar_cocoa_OBJC_O)
	xroar_CFLAGS += $(opt_cocoa_CFLAGS)
	xroar_OBJCFLAGS += $(opt_cocoa_OBJCFLAGS)
	xroar_LDFLAGS += $(opt_cocoa_LDFLAGS)
endif

xroar_alsa_C = src/alsa/ao_alsa.c
xroar_alsa_C_O = $(xroar_alsa_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_alsa_C)
xroar_CLEAN += $(xroar_alsa_C_O)
ifeq ($(opt_alsa),yes)
	xroar_C_O += $(xroar_alsa_C_O)
	xroar_CFLAGS += $(opt_alsa_CFLAGS)
	xroar_LDFLAGS += $(opt_alsa_LDFLAGS)
endif

xroar_oss_C = src/oss/ao_oss.c
xroar_oss_C_O = $(xroar_oss_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_oss_C)
xroar_CLEAN += $(xroar_oss_C_O)
ifeq ($(opt_oss),yes)
	xroar_C_O += $(xroar_oss_C_O)
	xroar_CFLAGS += $(opt_oss_CFLAGS)
	xroar_LDFLAGS += $(opt_oss_LDFLAGS)
endif

xroar_pulse_C = src/pulseaudio/ao_pulse.c
xroar_pulse_C_O = $(xroar_pulse_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_pulse_C)
xroar_CLEAN += $(xroar_pulse_C_O)
ifeq ($(opt_pulse),yes)
	xroar_C_O += $(xroar_pulse_C_O)
	xroar_CFLAGS += $(opt_pulse_CFLAGS)
	xroar_LDFLAGS += $(opt_pulse_LDFLAGS)
endif

xroar_sunaudio_C = src/sunos/ao_sun.c
xroar_sunaudio_C_O = $(xroar_sunaudio_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_sunaudio_C)
xroar_CLEAN += $(xroar_sunaudio_C_O)
ifeq ($(opt_sunaudio),yes)
	xroar_C_O += $(xroar_sunaudio_C_O)
	xroar_CFLAGS += $(opt_sunaudio_CFLAGS)
	xroar_LDFLAGS += $(opt_sunaudio_LDFLAGS)
endif

xroar_coreaudio_C = src/macosx/ao_macosx.c
xroar_coreaudio_C_O = $(xroar_coreaudio_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_coreaudio_C)
xroar_CLEAN += $(xroar_coreaudio_C_O)
ifeq ($(opt_coreaudio),yes)
	xroar_C_O += $(xroar_coreaudio_C_O)
	xroar_CFLAGS += $(opt_coreaudio_CFLAGS)
	xroar_LDFLAGS += $(opt_coreaudio_LDFLAGS)
endif

xroar_jack_C = src/jack/ao_jack.c
xroar_jack_C_O = $(xroar_jack_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_jack_C)
xroar_CLEAN += $(xroar_jack_C_O)
ifeq ($(opt_jack),yes)
	xroar_C_O += $(xroar_jack_C_O)
	xroar_CFLAGS += $(opt_jack_CFLAGS)
	xroar_LDFLAGS += $(opt_jack_LDFLAGS)
endif

xroar_sndfile_C = src/tape_sndfile.c
xroar_sndfile_C_O = $(xroar_sndfile_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_sndfile_C)
xroar_CLEAN += $(xroar_sndfile_C_O)
ifeq ($(opt_sndfile),yes)
	xroar_C_O += $(xroar_sndfile_C_O)
	xroar_CFLAGS += $(opt_sndfile_CFLAGS)
	xroar_LDFLAGS += $(opt_sndfile_LDFLAGS)
endif

xroar_nullaudio_C = src/ao_null.c
xroar_nullaudio_C_O = $(xroar_nullaudio_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_nullaudio_C)
xroar_CLEAN += $(xroar_nullaudio_C_O)
ifeq ($(opt_nullaudio),yes)
	xroar_C_O += $(xroar_nullaudio_C_O)
	xroar_CFLAGS += $(opt_nullaudio_CFLAGS)
	xroar_LDFLAGS += $(opt_nullaudio_LDFLAGS)
endif

xroar_linux_joystick_C = src/linux/joystick_linux.c
xroar_linux_joystick_C_O = $(xroar_linux_joystick_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_linux_joystick_C)
xroar_CLEAN += $(xroar_linux_joystick_C_O)
ifeq ($(opt_linux_joystick),yes)
	xroar_C_O += $(xroar_linux_joystick_C_O)
	xroar_CFLAGS += $(opt_linux_joystick_CFLAGS)
	xroar_LDFLAGS += $(opt_linux_joystick_LDFLAGS)
endif

xroar_mingw_C = \
	src/windows32/ao_windows32.c \
	src/windows32/common_windows32.c \
	src/windows32/filereq_windows32.c
xroar_mingw_C_O = $(xroar_mingw_C:.c=.o)
xroar_mingw_RC = src/windows32/xroar.rc
xroar_mingw_RC_RES = $(xroar_mingw_RC:.rc=.res)
#
xroar_SOURCES_C += $(xroar_mingw_C)
xroar_SOURCES_RC += $(xroar_mingw_RES)
xroar_CLEAN += $(xroar_mingw_C_O) $(xroar_mingw_RC_RES)
ifeq ($(opt_mingw),yes)
	xroar_C_O += $(xroar_mingw_C_O)
	xroar_RC_RES += $(xroar_mingw_RC_RES)
	xroar_CFLAGS += $(opt_mingw_CFLAGS)
	xroar_LDFLAGS += $(opt_mingw_LDFLAGS)
endif

xroar_trace_C = src/mc6809_trace.c src/hd6309_trace.c
xroar_trace_C_O = $(xroar_trace_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_trace_C)
xroar_CLEAN += $(xroar_trace_C_O)
ifeq ($(opt_trace),yes)
	xroar_C_O += $(xroar_trace_C_O)
	xroar_CFLAGS += $(opt_trace_CFLAGS)
	xroar_LDFLAGS += $(opt_trace_LDFLAGS)
endif

ifeq ($(opt_pthreads),yes)
	xroar_LDFLAGS += $(opt_pthreads_LDFLAGS)
endif

############################################################################
# Build rules

### Portalib

portalib_OBJS = \
	$(portalib_C_O)

$(portalib_OBJS): $(CONFIG_FILES)

$(portalib_C_O): %.o: $(SRCROOT)/%.c
	$(call do_cc,$@,$(portalib_CFLAGS) -c $<)

portalib/libporta.a: $(portalib_OBJS)
	$(call do_ar,$@,$(portalib_OBJS))
	$(call do_ranlib,$@)

portalib_CLEAN += portalib/libporta.a

build-lib: portalib/libporta.a

### XRoar

ifeq ($(opt_mingw),yes)
ROMPATH = \":~/Local\ Settings/Application\ Data/XRoar/roms:~/Application\ Data/XRoar/roms\"
CONFPATH = \":~/Local\ Settings/Application\ Data/XRoar:~/Application\ Data/XRoar\"
endif

ifeq ($(opt_coreaudio),yes)
ROMPATH = \"~/Library/XRoar/roms:~/.xroar/roms:$(datadir)/xroar/roms:\"
CONFPATH = \"~/Library/XRoar:~/.xroar:$(sysconfdir):$(datadir)/xroar\"
endif

ifndef ROMPATH
ROMPATH = \"~/.xroar/roms:$(datadir)/xroar/roms:\"
CONFPATH = \"~/.xroar:$(sysconfdir):$(datadir)/xroar\"
endif

xroar_OBJS = \
	$(xroar_C_O) \
	$(xroar_CXX_O) \
	$(xroar_OBJC_O) \
	$(xroar_RC_RES)

$(xroar_OBJS): $(CONFIG_FILES)

$(xroar_C_O): %.o: $(SRCROOT)/%.c
	$(call do_cc,$@,$(xroar_CFLAGS) -c $<)

$(xroar_CXX_O): %.o: $(SRCROOT)/%.cc
	$(call do_cc,$@,$(xroar_CXXFLAGS) -c $<)

$(xroar_OBJC_O): %.o: $(SRCROOT)/%.m
	$(call do_objc,$@,$(xroar_OBJCFLAGS) -c $<)

$(xroar_RC_RES): %.res: $(SRCROOT)/%.rc
	$(call do_windres,$@,-O coff -DVERSION=$(VERSION) -DVERSION_MAJOR=$(VERSION_MAJOR) -DVERSION_MINOR=$(VERSION_MINOR) -DVERSION_PATCH=$(VERSION_PATCH) -DVERSION_SUBPATCH=$(VERSION_SUBPATCH) $<)

src/xroar$(EXEEXT): $(xroar_OBJS) portalib/libporta.a
	$(call do_ld,$@,$(xroar_OBJS) portalib/libporta.a $(xroar_LDFLAGS))

xroar_CLEAN += src/xroar src/xroar.exe

build-bin: src/xroar$(EXEEXT)

############################################################################
# Documentation build rules

doc_CLEAN =

doc/version.texi: $(CONFIG_FILES)
	echo "@set VERSION $(VERSION)" > $@

doc/%.info: $(SRCROOT)/doc/%.texi doc/version.texi
	$(call do_makeinfo,$@,$<)

doc/%.pdf: $(SRCROOT)/doc/%.texi doc/version.texi
	$(call do_texi2pdf,$@,-q --build=clean $<)

doc/%.html: $(SRCROOT)/doc/%.texi doc/version.texi
	$(call do_makeinfo,$@,--html --no-headers --no-split $<)

doc/%.txt: $(SRCROOT)/doc/%.texi doc/version.texi
	$(call do_makeinfo,$@,--plaintext --no-headers --no-split $<)

doc_CLEAN += doc/xroar.info doc/xroar.pdf doc/xroar.html doc/xroar.txt

ifneq ($(MAKEINFO),)
build-doc: doc/xroar.info
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

.PHONY: install-bin
install-bin: build-bin
	$(INSTALL_DIR) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) src/xroar$(EXEEXT) $(DESTDIR)$(bindir)

.PHONY: install-doc
install-doc: build-doc
	@

.PHONY: install
install: install-bin install-doc

ifneq ($(MAKEINFO),)
install-doc: install-info
endif

.PHONY: install-info
install-info: doc/xroar.info
	$(INSTALL_DIR) $(DESTDIR)$(infodir)
	$(INSTALL_FILE) doc/xroar.info $(DESTDIR)$(infodir)

############################################################################
# Generated dependencies and the tools that generate them

tools_CLEAN =

# font2c

.SECONDARY: tools/font2c
tools/font2c: $(SRCROOT)/tools/font2c.c
	$(call do_build_cc,$@,$(opt_build_sdl_CFLAGS) $< $(opt_build_sdl_LDFLAGS) $(opt_build_sdl_image_LDFLAGS))

tools_CLEAN += tools/font2c

#

src/vdg_bitmaps.h: tools/font2c | $(SRCROOT)/src/font-6847.png $(SRCROOT)/src/font-6847t1.png
	tools/font2c --header --array font_6847 --type "unsigned char" --vdg $(SRCROOT)/src/font-6847.png > $@
	tools/font2c --header --array font_6847t1 --type "unsigned char" --vdgt1 $(SRCROOT)/src/font-6847t1.png >> $@

src/vdg_bitmaps.c: tools/font2c | $(SRCROOT)/src/font-6847.png
	tools/font2c --array font_6847 --type "unsigned char" --vdg $(SRCROOT)/src/font-6847.png > $@
	tools/font2c --array font_6847t1 --type "unsigned char" --vdgt1 $(SRCROOT)/src/font-6847t1.png >> $@

#

%.h: %.glade
	echo "static const gchar *$(@:%.h=%) =" | sed 's/\*.*\//\*/'> $@
	sed 's/"/'\''/g;s/^\( *\)/\1"/;s/$$/"/;' $< >> $@
	echo ";" >> $@

############################################################################
# Distribution creation

.PHONY: dist
dist:
	git archive --format=tar --output=../$(DISTNAME).tar --prefix=$(DISTNAME)/ HEAD
	gzip -f9 ../$(DISTNAME).tar

.PHONY: dist-w64
dist-w64: all doc/xroar.pdf
	mkdir $(DISTNAME)-w64
	cp $(SRCROOT)/COPYING.GPL $(SRCROOT)/ChangeLog $(SRCROOT)/README doc/xroar.pdf src/xroar.exe $(prefix)/bin/SDL.dll $(prefix)/bin/libsndfile-1.dll $(DISTNAME)-w64/
	cp $(SRCROOT)/COPYING.LGPL-2.1 $(DISTNAME)-w64/COPYING.LGPL-2.1
	$(TOOL_PREFIX)strip $(DISTNAME)-w64/xroar.exe
	$(TOOL_PREFIX)strip $(DISTNAME)-w64/SDL.dll
	$(TOOL_PREFIX)strip $(DISTNAME)-w64/libsndfile-1.dll
ifeq ($(opt_pthreads),yes)
	cp $(prefix)/bin/pthreadGC2.dll $(DISTNAME)-w64/
	$(TOOL_PREFIX)strip $(DISTNAME)-w64/pthreadGC2.dll
endif
	rm -f ../$(DISTNAME)-w64.zip
	zip -r ../$(DISTNAME)-w64.zip $(DISTNAME)-w64
	rm -rf $(DISTNAME)-w64/

.PHONY: dist-w32
dist-w32: all doc/xroar.pdf
	mkdir $(DISTNAME)-w32
	cp $(SRCROOT)/COPYING.GPL $(SRCROOT)/ChangeLog $(SRCROOT)/README doc/xroar.pdf src/xroar.exe $(prefix)/bin/SDL.dll $(prefix)/bin/libsndfile-1.dll $(DISTNAME)-w32/
	cp $(SRCROOT)/COPYING.LGPL-2.1 $(DISTNAME)-w32/COPYING.LGPL-2.1
	$(TOOL_PREFIX)strip $(DISTNAME)-w32/xroar.exe
	$(TOOL_PREFIX)strip $(DISTNAME)-w32/SDL.dll
	$(TOOL_PREFIX)strip $(DISTNAME)-w32/libsndfile-1.dll
ifeq ($(opt_pthreads),yes)
	cp $(prefix)/bin/pthreadGC2.dll $(DISTNAME)-w32/
	$(TOOL_PREFIX)strip $(DISTNAME)-w32/pthreadGC2.dll
endif
	rm -f ../$(DISTNAME)-w32.zip
	zip -r ../$(DISTNAME)-w32.zip $(DISTNAME)-w32
	rm -rf $(DISTNAME)-w32/

.PHONY: dist-macosx dist-macos
dist-macosx dist-macos: all doc/xroar.pdf
	mkdir XRoar-$(VERSION)
	mkdir -p XRoar-$(VERSION)/XRoar.app/Contents/MacOS XRoar-$(VERSION)/XRoar.app/Contents/Frameworks XRoar-$(VERSION)/XRoar.app/Contents/Resources
	cp src/xroar XRoar-$(VERSION)/XRoar.app/Contents/MacOS/
	cp /usr/local/lib/libSDL-1.2.0.dylib XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/
	chmod 0644 XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/libSDL-1.2.0.dylib
	install_name_tool -change /usr/local/lib/libSDL-1.2.0.dylib @executable_path/../Frameworks/libSDL-1.2.0.dylib XRoar-$(VERSION)/XRoar.app/Contents/MacOS/xroar
	cp /usr/local/lib/libsndfile.1.dylib XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/
	chmod 0644 XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/libsndfile.1.dylib
	install_name_tool -change /usr/local/lib/libsndfile.1.dylib @executable_path/../Frameworks/libsndfile.1.dylib XRoar-$(VERSION)/XRoar.app/Contents/MacOS/xroar
	strip XRoar-$(VERSION)/XRoar.app/Contents/MacOS/xroar
	strip -x XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/libSDL-1.2.0.dylib
	strip -x XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/libsndfile.1.dylib
	sed -e "s!@VERSION@!$(VERSION)!g" $(SRCROOT)/src/macosx/Info.plist.in > XRoar-$(VERSION)/XRoar.app/Contents/Info.plist
	cp $(SRCROOT)/src/macosx/xroar.icns XRoar-$(VERSION)/XRoar.app/Contents/Resources/
	cp $(SRCROOT)/README $(SRCROOT)/COPYING.GPL $(SRCROOT)/ChangeLog doc/xroar.pdf XRoar-$(VERSION)/
	cp $(SRCROOT)/COPYING.LGPL-2.1 XRoar-$(VERSION)/COPYING.LGPL-2.1
	chmod -R o+rX,g+rX XRoar-$(VERSION)/
	hdiutil create -srcfolder XRoar-$(VERSION) -uid 99 -gid 99 ../XRoar-$(VERSION).dmg
	rm -rf XRoar-$(VERSION)/

.PHONY: debuild
debuild: dist
	-cd ..; rm -rf $(DISTNAME)/ $(DISTNAME).orig/
	cd ..; mv $(DISTNAME).tar.gz xroar_$(VERSION).orig.tar.gz
	cd ..; tar xfz xroar_$(VERSION).orig.tar.gz
	rsync -axH debian --exclude='debian/.git/' --exclude='debian/_darcs/' ../$(DISTNAME)/
	cd ../$(DISTNAME); debuild

############################################################################
# Clean-up, etc.

.PHONY: clean
clean:
	rm -f $(portalib_CLEAN)
	rm -f $(xroar_CLEAN)
	rm -f $(doc_CLEAN)
	rm -f $(tools_CLEAN)

.PHONY: profile-clean
profile-clean:
	rm -f $(xroar_OBJS:.o=.gcda) $(portalib_OBJS:.o=.gcda)
	rm -f $(xroar_OBJS:.o=.gcno) $(portalib_OBJS:.o=.gcno)

.PHONY: distclean
distclean: clean profile-clean
	rm -f config.h config.mak config.log

.PHONY: depend
depend:
	@touch .deps.mak
	makedepend -f .deps.mak -Y $(portalib_SOURCES_C) $(xroar_SOURCES_C) 2> /dev/null

-include .deps.mak
