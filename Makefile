# Makefile for XRoar.
# Needs you to run './configure' first.  './configure --help' for help.

-include config.mak

VERSION := 0.23
DISTNAME = xroar-$(VERSION)

.PHONY: all
all: build-bin build-doc

############################################################################
# Base object files and settings required by all builds

WARN = -Wall -W -Wstrict-prototypes -Wpointer-arith -Wcast-align \
	-Wcast-qual -Wshadow -Waggregate-return -Wnested-externs -Winline \
	-Wwrite-strings -Wundef -Wsign-compare -Wmissing-prototypes \
	-Wredundant-decls

CONFIG_FILES = config.h config.mak

# Objects common to all builds:
xroar_common_OBJS = crc16.o path.o portalib.o xconfig.o \
	cart.o deltados.o dragondos.o events.o hexs19.o input.o joystick.o \
	keyboard.o m6809.o machine.o mc6821.o module.o rsdos.o sam.o \
	snapshot.o tape.o vdg.o vdisk.o vdrive.o wd279x.o xroar.o
xroar_common_INT_OBJS = vdg_bitmaps.o
CLEAN = $(xroar_common_OBJS) $(xroar_common_INT_OBJS)

# Objects for all Unix-style builds (the default):
xroar_unix_OBJS = fs_unix.o main_unix.o
xroar_unix_INT_OBJS =
CLEAN += $(xroar_unix_OBJS) $(xroar_unix_INT_OBJS)

############################################################################
# Optional extras (most only apply to Unix-style build)

xroar_opt_OBJS =
xroar_opt_INT_OBJS =

opt_sdl_OBJS = ui_sdl.o video_sdl.o video_sdlyuv.o sound_sdl.o keyboard_sdl.o \
		joystick_sdl.o
CLEAN += $(opt_sdl_OBJS)
ifeq ($(CONFIG_SDL),yes)
	xroar_opt_OBJS += $(opt_sdl_OBJS)
video_sdl.o: vdg_bitmaps.h
video_sdlyuv.o: vdg_bitmaps.h
endif

opt_sdlgl_OBJS = video_sdlgl.o
CLEAN += $(opt_sdlgl_OBJS)
ifeq ($(CONFIG_SDLGL),yes)
	xroar_opt_OBJS += $(opt_sdlgl_OBJS)
video_sdlgl.o: vdg_bitmaps.h
endif

opt_curses_OBJS = ui_curses.o video_curses.o keyboard_curses.o
CLEAN += $(opt_curses_OBJS)
ifeq ($(CONFIG_CURSES),yes)
	xroar_opt_OBJS += $(opt_curses_OBJS)
endif

opt_gtk2_OBJS = filereq_gtk2.o
CLEAN += $(opt_gtk2_OBJS)
ifeq ($(CONFIG_GTK2),yes)
	xroar_opt_OBJS += $(opt_gtk2_OBJS)
endif

opt_gtk1_OBJS = filereq_gtk1.o
CLEAN += $(opt_gtk1_OBJS)
ifeq ($(CONFIG_GTK1),yes)
	xroar_opt_OBJS += $(opt_gtk1_OBJS)
endif

opt_cli_OBJS = filereq_cli.o
CLEAN += $(opt_cli_OBJS)
ifeq ($(CONFIG_CLI),yes)
	xroar_opt_OBJS += $(opt_cli_OBJS)
endif

opt_carbon_OBJS = filereq_carbon.o
CLEAN += $(opt_carbon_OBJS)
ifeq ($(CONFIG_CARBON),yes)
	xroar_opt_OBJS += $(opt_carbon_OBJS)
endif

opt_alsa_OBJS = sound_alsa.o
CLEAN += $(opt_alsa_OBJS)
ifeq ($(CONFIG_ALSA),yes)
	xroar_opt_OBJS += $(opt_alsa_OBJS)
endif

opt_oss_OBJS = sound_oss.o
CLEAN += $(opt_oss_OBJS)
ifeq ($(CONFIG_OSS),yes)
	xroar_opt_OBJS += $(opt_oss_OBJS)
endif

opt_sunaudio_OBJS = sound_sun.o
CLEAN += $(opt_sunaudio_OBJS)
ifeq ($(CONFIG_SUN),yes)
	xroar_opt_OBJS += $(opt_sunaudio_OBJS)
endif

opt_coreaudio_OBJS = sound_macosx.o
CLEAN += $(opt_coreaudio_OBJS)
ifeq ($(CONFIG_COREAUDIO),yes)
	xroar_opt_OBJS += $(opt_coreaudio_OBJS)
endif

opt_jack_OBJS = sound_jack.o
CLEAN += $(opt_jack_OBJS)
ifeq ($(CONFIG_JACK),yes)
	xroar_opt_OBJS += $(opt_jack_OBJS)
endif

opt_nullaudio_OBJS = sound_null.o
CLEAN += $(opt_nullaudio_OBJS)
ifeq ($(CONFIG_NULLAUDIO),yes)
	xroar_opt_OBJS += $(opt_nullaudio_OBJS)
endif

opt_mingw_OBJS = common_windows32.o filereq_windows32.o
CLEAN += $(opt_mingw_OBJS)
ifeq ($(CONFIG_MINGW),yes)
	xroar_opt_OBJS += $(opt_mingw_OBJS)
endif

opt_trace_OBJS = m6809_trace.o
CLEAN += $(opt_trace_OBJS)
ifeq ($(CONFIG_TRACE),yes)
	xroar_opt_OBJS += $(opt_trace_OBJS)
endif

############################################################################
# Build rules

# GP32 rules
include makefile_gp32.mk

# Nintendo DS rules
include makefile_nds.mk

# Unix rules (default)
ifeq ($(BUILD_STYLE),)

ifeq ($(CONFIG_MINGW),yes)
ROMPATH = \".\"
CONFPATH = \".\"
else
ROMPATH = \":~/.xroar/roms:~/Library/XRoar/Roms:$(datadir)/xroar/roms\"
CONFPATH = \":~/.xroar:~/Library/XRoar:$(datadir)/xroar\"
endif

xroar_unix_CFLAGS = $(CFLAGS)
xroar_unix_CPPFLAGS = $(CPPFLAGS) -I$(CURDIR) -I$(SRCROOT) $(WARN) \
        -DVERSION=\"$(VERSION)\" \
        -DROMPATH=$(ROMPATH) -DCONFPATH=$(CONFPATH)
xroar_unix_LDFLAGS = $(LDFLAGS)
xroar_unix_LDLIBS = $(LDLIBS)

xroar_unix_ALL_OBJS = $(xroar_common_OBJS) $(xroar_common_INT_OBJS) \
	$(xroar_unix_OBJS) $(xroar_unix_INT_OBJS) \
	$(xroar_opt_OBJS) $(xroar_opt_INT_OBJS)

$(xroar_unix_ALL_OBJS): $(CONFIG_FILES)

$(xroar_common_OBJS) $(xroar_unix_OBJS) $(xroar_opt_OBJS): %.o: $(SRCROOT)/%.c
	$(CC) $(xroar_unix_CFLAGS) $(xroar_unix_CPPFLAGS) -c $<

$(xroar_common_INT_OBJS) $(xroar_unix_INT_OBJS) $(xroar_opt_INT_OBJS): %.o: ./%.c
	$(CC) $(xroar_unix_CFLAGS) $(xroar_unix_CPPFLAGS) -c $<

xroar$(EXEEXT): $(xroar_unix_ALL_OBJS)
	$(CC) -o $@ $(xroar_unix_ALL_OBJS) $(xroar_unix_LDFLAGS) $(xroar_unix_LDLIBS)

.PHONY: build-bin
build-bin: xroar$(EXEEXT)

endif

CLEAN += xroar xroar.exe

############################################################################
# Documentation build rules

doc/xroar.info: $(SRCROOT)/doc/xroar.texi
	$(MAKEINFO) -D "VERSION $(VERSION)" -o $@ $<

doc/xroar.pdf: $(SRCROOT)/doc/xroar.texi
	$(TEXI2PDF) -t "@set VERSION $(VERSION)" --build=clean -o $@ $<

doc/xroar.html: $(SRCROOT)/doc/xroar.texi
	$(MAKEINFO) --html --no-headers --no-split -D "VERSION $(VERSION)" -o $@ $<

doc/xroar.txt: $(SRCROOT)/doc/xroar.texi
	$(MAKEINFO) --plaintext --no-headers --no-split -D "VERSION $(VERSION)" -o $@ $<

CLEAN += doc/xroar.info doc/xroar.pdf doc/xroar.html doc/xroar.txt

.PHONY: build-doc
build-doc:
	@

ifneq ($(MAKEINFO),)
build-doc: doc/xroar.info
endif

############################################################################
# Install rules

ifeq ($(CONFIG_INSTALL),yes)
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
	$(INSTALL_PROGRAM) $(TARGET) $(DESTDIR)$(bindir)

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

tools/font2c: $(SRCROOT)/tools/font2c.c
	mkdir -p tools
	$(BUILD_CC) $(BUILD_SDL_CFLAGS) -o $@ $< $(BUILD_SDL_LDFLAGS) $(BUILD_SDL_IMAGE_LDFLAGS)

CLEAN += tools/font2c

#

vdg_bitmaps.h: tools/font2c $(SRCROOT)/vdgfont.png
	tools/font2c --header --array vdg_alpha --type "unsigned int" --vdg $(SRCROOT)/vdgfont.png > $@

vdg_bitmaps.c: tools/font2c $(SRCROOT)/vdgfont.png
	tools/font2c --array vdg_alpha --type "unsigned int" --vdg $(SRCROOT)/vdgfont.png > $@

CLEAN += vdg_bitmaps.h vdg_bitmaps.c

############################################################################
# Distribution creation

.PHONY: dist
dist:
	git archive --format=tar --output=../$(DISTNAME).tar --prefix=$(DISTNAME)/ HEAD
	gzip -f9 ../$(DISTNAME).tar

.PHONY: dist-windows32
dist-windows32: all doc/xroar.pdf
	mkdir $(DISTNAME)-windows32
	cp COPYING.GPL ChangeLog README doc/xroar.pdf xroar.exe /usr/local/$(TARGET_ARCH)/bin/SDL.dll /usr/local/$(TARGET_ARCH)/bin/libsndfile-1.dll $(DISTNAME)-windows32/
	cp COPYING.LGPL-2.1 $(DISTNAME)-windows32/COPYING.LGPL-2.1
	$(TOOL_PREFIX)strip $(DISTNAME)-windows32/xroar.exe
	$(TOOL_PREFIX)strip $(DISTNAME)-windows32/SDL.dll
	$(TOOL_PREFIX)strip $(DISTNAME)-windows32/libsndfile-1.dll
	rm -f ../$(DISTNAME)-windows32.zip
	zip -r ../$(DISTNAME)-windows32.zip $(DISTNAME)-windows32
	rm -rf $(DISTNAME)-windows32/

.PHONY: dist-macosx dist-macos
dist-macosx dist-macos: all doc/xroar.pdf
	mkdir XRoar-$(VERSION)
	mkdir -p XRoar-$(VERSION)/XRoar.app/Contents/MacOS XRoar-$(VERSION)/XRoar.app/Contents/Frameworks XRoar-$(VERSION)/XRoar.app/Contents/Resources
	cp xroar XRoar-$(VERSION)/XRoar.app/Contents/MacOS/
	cp /usr/local/lib/libSDL-1.2.0.dylib XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/
	install_name_tool -change /usr/local/lib/libSDL-1.2.0.dylib @executable_path/../Frameworks/libSDL-1.2.0.dylib XRoar-$(VERSION)/XRoar.app/Contents/MacOS/xroar
	cp /usr/local/lib/libsndfile.1.dylib XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/
	install_name_tool -change /usr/local/lib/libsndfile.1.dylib @executable_path/../Frameworks/libsndfile.1.dylib XRoar-$(VERSION)/XRoar.app/Contents/MacOS/xroar
	strip XRoar-$(VERSION)/XRoar.app/Contents/MacOS/xroar
	strip -x XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/libSDL-1.2.0.dylib
	strip -x XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/libsndfile.1.dylib
	sed -e "s!@VERSION@!$(VERSION)!g" macos/Info.plist.in > XRoar-$(VERSION)/XRoar.app/Contents/Info.plist
	cp macos/xroar.icns XRoar-$(VERSION)/XRoar.app/Contents/Resources/
	cp README COPYING.GPL ChangeLog doc/xroar.pdf XRoar-$(VERSION)/
	cp COPYING.LGPL-2.1 XRoar-$(VERSION)/COPYING.LGPL-2.1
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

.PHONY: distclean
distclean: clean
	rm -f config.h config.mak config.log

.PHONY: depend
depend:
	@touch .deps.mak
	makedepend -f .deps.mak -Y `git ls-files | sort | grep '\.c'` 2> /dev/null

-include .deps.mak
