# Makefile for XRoar.
# Needs you to run './configure' first.  './configure --help' for help.

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
all: build-bin build-doc

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
do_ld = $(CC) -o $(1) $(2)
do_build_cc = $(BUILD_CC) -o $(1) $(2)
do_build_cxx = $(BUILD_CXX) -o $(1) $(2)
do_build_objc = $(BUILD_OBJC) -o $(1) $(2)
do_windres = $(WINDRES) -o $(1) $(2)
do_makeinfo = $(MAKEINFO) -o $(1) $(2)
do_texi2pdf = $(TEXI2PDF) -o $(1) $(2)

endif

CONFIG_FILES = config.h config.mak

CLEAN =

### Portalib

portalib_CFLAGS = $(CFLAGS) $(CPPFLAGS) \
	-I$(CURDIR) -I$(SRCROOT) $(WARN)

portalib_BASE_OBJS = \
	portalib/strsep.o
CLEAN += $(portalib_BASE_OBJS)
portalib_OBJS_C = $(portalib_BASE_OBJS)

### XRoar

xroar_CFLAGS = $(CFLAGS) $(CPPFLAGS) \
	-I$(CURDIR) -I$(SRCROOT) -I$(SRCROOT)/portalib -I$(SRCROOT)/src \
	$(WARN) \
        -DVERSION=\"$(VERSION)\" \
        -DROMPATH=$(ROMPATH) -DCONFPATH=$(CONFPATH)
xroar_CXXFLAGS = $(CXXFLAGS) $(CPPFLAGS) \
	-I$(CURDIR) -I$(SRCROOT) -I$(SRCROOT)/portalib -I$(SRCROOT)/src \
	$(WARN) \
        -DVERSION=\"$(VERSION)\" \
        -DROMPATH=$(ROMPATH) -DCONFPATH=$(CONFPATH)
xroar_OBJCFLAGS = $(OBJCFLAGS) $(CPPFLAGS) \
	-I$(CURDIR) -I$(SRCROOT) -I$(SRCROOT)/portalib -I$(SRCROOT)/src \
	$(WARN) \
        -DVERSION=\"$(VERSION)\" \
        -DROMPATH=$(ROMPATH) -DCONFPATH=$(CONFPATH)
xroar_LDFLAGS = $(LDFLAGS) $(LDLIBS)

xroar_BASE_OBJS_C = \
	src/becker.o \
	src/breakpoint.o \
	src/cart.o \
	src/crc16.o \
	src/crc32.o \
	src/crclist.o \
	src/deltados.o \
	src/dragondos.o \
	src/events.o \
	src/fs.o \
	src/hd6309.o \
	src/hexs19.o \
	src/input.o \
	src/joystick.o \
	src/keyboard.o \
	src/logging.o \
	src/machine.o \
	src/mc6809.o \
	src/mc6821.o \
	src/module.o \
	src/path.o \
	src/printer.o \
	src/romlist.o \
	src/rsdos.o \
	src/sam.o \
	src/snapshot.o \
	src/sound.o \
	src/tape.o \
	src/tape_cas.o \
	src/ui_null.o \
	src/vdg.o \
	src/vdg_palette.o \
	src/vdisk.o \
	src/vdrive.o \
	src/vo_null.o \
	src/wd279x.o \
	src/xconfig.o \
	src/xroar.o
xroar_BASE_INT_OBJS_C = src/vdg_bitmaps.o
xroar_BASE_OBJS_CXX =
xroar_BASE_OBJS_OBJC =
CLEAN += $(xroar_BASE_OBJS_C) $(xroar_BASE_INT_OBJS_C) \
	$(xroar_BASE_OBJS_CXX) $(xroar_BASE_OBJS_OBJC)
xroar_OBJS_C = $(xroar_BASE_OBJS_C)
xroar_INT_OBJS_C = $(xroar_BASE_INT_OBJS_C)
xroar_OBJS_CXX = $(xroar_BASE_OBJS_CXX)
xroar_OBJS_OBJC = $(xroar_BASE_OBJS_OBJC)
xroar_RES =

src/vdg.o: src/vdg_bitmaps.h

# Objects for all Unix-style builds (now the only one):
xroar_unix_OBJS = src/main_unix.o
CLEAN += $(xroar_unix_OBJS)
xroar_OBJS_C += $(xroar_unix_OBJS)

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

portalib_glib2_OBJS_C = \
	portalib/glib/ghash.o \
	portalib/glib/gmem.o \
	portalib/glib/gslist.o \
	portalib/glib/gstrfuncs.o
CLEAN += $(portalib_glib2_OBJS_C)
ifneq ($(opt_glib2),yes)
	portalib_OBJS_C += $(portalib_glib2_OBJS_C)
	portalib_CFLAGS += $(opt_glib2_CFLAGS)
endif

xroar_gdb_OBJS_C = src/gdb.o
CLEAN += $(xroar_gdb_OBJS_C)
ifeq ($(opt_gdb_stub),yes)
	xroar_OBJS_C += $(xroar_gdb_OBJS_C)
endif

ifeq ($(opt_glib2),yes)
	xroar_CFLAGS += $(opt_glib2_CFLAGS)
	xroar_OBJCFLAGS += $(opt_glib2_CFLAGS)
	xroar_LDFLAGS += $(opt_glib2_LDFLAGS)
endif

xroar_gtk2_OBJS_C = \
	src/gtk2/drivecontrol.o \
	src/gtk2/filereq_gtk2.o \
	src/gtk2/keyboard_gtk2.o \
	src/gtk2/tapecontrol.o \
	src/gtk2/ui_gtk2.o
CLEAN += $(xroar_gtk2_OBJS_C)
ifeq ($(opt_gtk2),yes)
	xroar_OBJS_C += $(xroar_gtk2_OBJS_C)
	xroar_CFLAGS += $(opt_gtk2_CFLAGS)
	xroar_LDFLAGS += $(opt_gtk2_LDFLAGS)
src/gtk2/drivecontrol.o: src/gtk2/drivecontrol_glade.h
src/gtk2/tapecontrol.o: src/gtk2/tapecontrol_glade.h
src/gtk2/ui_gtk2.o: src/gtk2/top_window_glade.h
src/keyboard_gtk2.o: $(SRCROOT)/src/gtk2/keyboard_gtk2_mappings.c
endif

xroar_gtkgl_OBJS_C = src/gtk2/vo_gtkgl.o
CLEAN += $(xroar_gtkgl_OBJS_C)
ifeq ($(opt_gtkgl),yes)
	xroar_OBJS_C += $(xroar_gtkgl_OBJS_C)
	xroar_CFLAGS += $(opt_gtkgl_CFLAGS)
	xroar_LDFLAGS += $(opt_gtkgl_LDFLAGS)
endif

xroar_opengl_OBJS_C = src/vo_opengl.o
CLEAN += $(xroar_opengl_OBJS_C)
ifeq ($(opt_opengl),yes)
	xroar_OBJS_C += $(xroar_opengl_OBJS_C)
	xroar_CFLAGS += $(opt_opengl_CFLAGS)
	xroar_LDFLAGS += $(opt_opengl_LDFLAGS)
endif

xroar_sdl_OBJS_C = \
	src/sdl/ao_sdl.o \
	src/sdl/common.o \
	src/sdl/joystick_sdl.o \
	src/sdl/keyboard_sdl.o \
	src/sdl/ui_sdl.o \
	src/sdl/vo_sdl.o \
	src/sdl/vo_sdlyuv.o
CLEAN += $(xroar_sdl_OBJS_C)
ifeq ($(opt_sdl),yes)
	xroar_OBJS_C += $(xroar_sdl_OBJS_C)
	xroar_CFLAGS += $(opt_sdl_CFLAGS)
	xroar_OBJCFLAGS += $(opt_sdl_OBJCFLAGS)
	xroar_LDFLAGS += $(opt_sdl_LDFLAGS)
src/sdl/keyboard_sdl.o: $(SRCROOT)/src/sdl/keyboard_sdl_mappings.c
endif

xroar_sdlgl_OBJS_C = src/sdl/vo_sdlgl.o
CLEAN += $(xroar_sdlgl_OBJS_C)
ifeq ($(opt_sdlgl),yes)
	xroar_OBJS_C += $(xroar_sdlgl_OBJS_C)
	xroar_CFLAGS += $(opt_sdlgl_CFLAGS)
	xroar_LDFLAGS += $(opt_sdlgl_LDFLAGS)
endif

xroar_cli_OBJS_C = src/filereq_cli.o
CLEAN += $(xroar_cli_OBJS_C)
ifeq ($(opt_cli),yes)
	xroar_OBJS_C += $(xroar_cli_OBJS_C)
	xroar_CFLAGS += $(opt_cli_CFLAGS)
	xroar_LDFLAGS += $(opt_cli_LDFLAGS)
endif

xroar_cocoa_OBJS_OBJC = \
	src/macosx/filereq_cocoa.o \
	src/macosx/ui_macosx.o
CLEAN += $(xroar_cocoa_OBJS_OBJC)
ifeq ($(opt_cocoa),yes)
	xroar_OBJS_OBJC += $(xroar_cocoa_OBJS_OBJC)
	xroar_CFLAGS += $(opt_cocoa_CFLAGS)
	xroar_OBJCFLAGS += $(opt_cocoa_OBJCFLAGS)
	xroar_LDFLAGS += $(opt_cocoa_LDFLAGS)
endif

xroar_alsa_OBJS_C = src/alsa/ao_alsa.o
CLEAN += $(xroar_alsa_OBJS_C)
ifeq ($(opt_alsa),yes)
	xroar_OBJS_C += $(xroar_alsa_OBJS_C)
	xroar_CFLAGS += $(opt_alsa_CFLAGS)
	xroar_LDFLAGS += $(opt_alsa_LDFLAGS)
endif

xroar_oss_OBJS_C = src/oss/ao_oss.o
CLEAN += $(xroar_oss_OBJS_C)
ifeq ($(opt_oss),yes)
	xroar_OBJS_C += $(xroar_oss_OBJS_C)
	xroar_CFLAGS += $(opt_oss_CFLAGS)
	xroar_LDFLAGS += $(opt_oss_LDFLAGS)
endif

xroar_pulse_OBJS_C = src/pulseaudio/ao_pulse.o
CLEAN += $(xroar_pulse_OBJS_C)
ifeq ($(opt_pulse),yes)
	xroar_OBJS_C += $(xroar_pulse_OBJS_C)
	xroar_CFLAGS += $(opt_pulse_CFLAGS)
	xroar_LDFLAGS += $(opt_pulse_LDFLAGS)
endif

xroar_sunaudio_OBJS_C = src/sunos/ao_sun.o
CLEAN += $(xroar_sunaudio_OBJS_C)
ifeq ($(opt_sunaudio),yes)
	xroar_OBJS_C += $(xroar_sunaudio_OBJS_C)
	xroar_CFLAGS += $(opt_sunaudio_CFLAGS)
	xroar_LDFLAGS += $(opt_sunaudio_LDFLAGS)
endif

xroar_coreaudio_OBJS_C = src/macosx/ao_macosx.o
CLEAN += $(xroar_coreaudio_OBJS_C)
ifeq ($(opt_coreaudio),yes)
	xroar_OBJS_C += $(xroar_coreaudio_OBJS_C)
	xroar_CFLAGS += $(opt_coreaudio_CFLAGS)
	xroar_LDFLAGS += $(opt_coreaudio_LDFLAGS)
endif

xroar_jack_OBJS_C = src/jack/ao_jack.o
CLEAN += $(xroar_jack_OBJS_C)
ifeq ($(opt_jack),yes)
	xroar_OBJS_C += $(xroar_jack_OBJS_C)
	xroar_CFLAGS += $(opt_jack_CFLAGS)
	xroar_LDFLAGS += $(opt_jack_LDFLAGS)
endif

xroar_sndfile_OBJS_C = src/tape_sndfile.o
CLEAN += $(xroar_sndfile_OBJS_C)
ifeq ($(opt_sndfile),yes)
	xroar_OBJS_C += $(xroar_sndfile_OBJS_C)
	xroar_CFLAGS += $(opt_sndfile_CFLAGS)
	xroar_LDFLAGS += $(opt_sndfile_LDFLAGS)
endif

xroar_nullaudio_OBJS_C = src/ao_null.o
CLEAN += $(xroar_nullaudio_OBJS_C)
ifeq ($(opt_nullaudio),yes)
	xroar_OBJS_C += $(xroar_nullaudio_OBJS_C)
	xroar_CFLAGS += $(opt_nullaudio_CFLAGS)
	xroar_LDFLAGS += $(opt_nullaudio_LDFLAGS)
endif

xroar_linux_joystick_OBJS_C = src/linux/joystick_linux.o
CLEAN += $(xroar_linux_joystick_OBJS_C)
ifeq ($(opt_linux_joystick),yes)
	xroar_OBJS_C += $(xroar_linux_joystick_OBJS_C)
	xroar_CFLAGS += $(opt_linux_joystick_CFLAGS)
	xroar_LDFLAGS += $(opt_linux_joystick_LDFLAGS)
endif

xroar_mingw_OBJS_C = \
	src/windows32/ao_windows32.o \
	src/windows32/common_windows32.o \
	src/windows32/filereq_windows32.o
xroar_mingw_RES = src/windows32/xroar.res
CLEAN += $(xroar_mingw_OBJS_C) $(xroar_mingw_RES)
ifeq ($(opt_mingw),yes)
	xroar_OBJS_C += $(xroar_mingw_OBJS_C)
	xroar_RES += $(xroar_mingw_RES)
	xroar_CFLAGS += $(opt_mingw_CFLAGS)
	xroar_LDFLAGS += $(opt_mingw_LDFLAGS)
endif

xroar_trace_OBJS_C = src/mc6809_trace.o src/hd6309_trace.o
CLEAN += $(xroar_trace_OBJS_C)
ifeq ($(opt_trace),yes)
	xroar_OBJS_C += $(xroar_trace_OBJS_C)
	xroar_CFLAGS += $(opt_trace_CFLAGS)
	xroar_LDFLAGS += $(opt_trace_LDFLAGS)
endif

ifeq ($(opt_pthreads),yes)
	xroar_LDFLAGS += $(opt_pthreads_LDFLAGS)
endif

############################################################################
# Build rules

# Unix rules (default)
ifeq ($(BUILD_STYLE),)

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

xroar_OBJS = $(xroar_OBJS_C) $(xroar_INT_OBJS_C) \
	$(xroar_OBJS_CXX) \
	$(xroar_OBJS_OBJC) \
	$(xroar_RES)

portalib_OBJS = $(portalib_OBJS_C)

$(portalib_OBJS): $(CONFIG_FILES)

$(portalib_OBJS_C): %.o: $(SRCROOT)/%.c
	$(call do_cc,$@,$(portalib_CFLAGS) -c $<)

$(xroar_OBJS): $(CONFIG_FILES)

$(xroar_OBJS_C): %.o: $(SRCROOT)/%.c
	$(call do_cc,$@,$(xroar_CFLAGS) -c $<)

$(xroar_OBJS_CXX): %.o: $(SRCROOT)/%.cc
	$(call do_cxx,$@,$(xroar_CXXFLAGS) -c $<)

$(xroar_OBJS_OBJC): %.o: $(SRCROOT)/%.m
	$(call do_objc,$@,$(xroar_OBJCFLAGS) -c $<)

$(xroar_INT_OBJS_C): %.o: ./%.c
	$(call do_cc,$@,$(xroar_CFLAGS) -c $<)

src/xroar$(EXEEXT): $(xroar_OBJS) $(portalib_OBJS)
	$(call do_ld,$@,$(xroar_OBJS) $(portalib_OBJS) $(xroar_LDFLAGS))

.PHONY: build-bin
build-bin: src/xroar$(EXEEXT)

windows32/xroar.res: $(SRCROOT)/windows32/xroar.rc
	$(call do_windres,$@,-O coff -DVERSION=$(VERSION) -DVERSION_MAJOR=$(VERSION_MAJOR) -DVERSION_MINOR=$(VERSION_MINOR) -DVERSION_PATCH=$(VERSION_PATCH) -DVERSION_SUBPATCH=$(VERSION_SUBPATCH) $<)

endif

CLEAN += src/xroar src/xroar.exe

############################################################################
# Documentation build rules

doc/%.info: $(SRCROOT)/doc/%.texi
	$(call do_makeinfo,$@,-D "VERSION $(VERSION)" $<)

doc/%.pdf: $(SRCROOT)/doc/%.texi
	$(call do_texi2pdf,$@,-q -t "@set VERSION $(VERSION)" --build=clean $<)

doc/%.html: $(SRCROOT)/doc/%.texi
	$(call do_makeinfo,$@,--html --no-headers --no-split -D "VERSION $(VERSION)" $<)

doc/%.txt: $(SRCROOT)/doc/%.texi
	$(call do_makeinfo,$@,--plaintext --no-headers --no-split -D "VERSION $(VERSION)" $<)

CLEAN += doc/xroar.info doc/xroar.pdf doc/xroar.html doc/xroar.txt

.PHONY: build-doc
build-doc:
	@

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

.SECONDARY: tools/font2c
tools/font2c: $(SRCROOT)/tools/font2c.c
	$(call do_build_cc,$@,$(opt_build_sdl_CFLAGS) $< $(opt_build_sdl_LDFLAGS) $(opt_build_sdl_image_LDFLAGS))

CLEAN += tools/font2c

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
	sed -e "s!@VERSION@!$(VERSION)!g" macosx/Info.plist.in > XRoar-$(VERSION)/XRoar.app/Contents/Info.plist
	cp $(SRCROOT)/macosx/xroar.icns XRoar-$(VERSION)/XRoar.app/Contents/Resources/
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
	rm -f $(CLEAN)

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
	makedepend -f .deps.mak -Y `git ls-files | sort | grep '\.c'` 2> /dev/null

-include .deps.mak
