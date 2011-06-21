# Makefile for XRoar.
# Needs you to run './configure' first.  './configure --help' for help.

# Run with "VERBOSE=1" for normal output.

-include config.mak

#VERBOSE = 1
VERSION := 0.25.1
DISTNAME = xroar-$(VERSION)

.PHONY: all
all: build-bin build-doc

############################################################################
# Base object files and settings required by all builds

ifeq ($(VERBOSE),)

WARN = -Wall -W

do_cc = @echo CC $(1); $(CC) -o $(1) $(2)
do_objc = @echo OBJC $(1); $(OBJC) -o $(1) $(2)
do_build_cc = @echo BUILD_CC $(1); $(BUILD_CC) -o $(1) $(2)
do_build_objc = @echo BUILD_OBJC $(1); $(BUILD_OBJC) -o $(1) $(2)
do_makeinfo = @echo MAKEINFO $(1); $(MAKEINFO) -o $(1) $(2)
do_texi2pdf = @echo TEXI2PDF $(1); $(TEXI2PDF) -o $(1) $(2)

else

WARN = -Wall -W -Wstrict-prototypes -Wpointer-arith -Wcast-align \
	-Wcast-qual -Wshadow -Waggregate-return -Wnested-externs -Winline \
	-Wwrite-strings -Wundef -Wmissing-prototypes -Wredundant-decls

do_cc = $(CC) -o $(1) $(2)
do_objc = $(OBJC) -o $(1) $(2)
do_build_cc = $(BUILD_CC) -o $(1) $(2)
do_build_objc = $(BUILD_OBJC) -o $(1) $(2)
do_makeinfo = $(MAKEINFO) -o $(1) $(2)
do_texi2pdf = $(TEXI2PDF) -o $(1) $(2)

endif

CONFIG_FILES = config.h config.mak

# Objects common to all builds:
xroar_common_OBJS = crc16.o list.o misc.o path.o portalib.o xconfig.o \
	breakpoint.o cart.o deltados.o dragondos.o events.o hexs19.o input.o \
	joystick.o keyboard.o m6809.o machine.o mc6821.o module.o printer.o \
	rsdos.o sam.o snapshot.o sound.o tape.o tape_cas.o ui_null.o vdg.o \
	vdg_palette.o vdisk.o vdrive.o vo_null.o wd279x.o xroar.o
xroar_common_INT_OBJS = vdg_bitmaps.o
CLEAN = $(xroar_common_OBJS) $(xroar_common_INT_OBJS)

# Objects for all Unix-style builds (the default):
xroar_unix_OBJS = fs_unix.o main_unix.o
xroar_unix_INT_OBJS =
CLEAN += $(xroar_unix_OBJS) $(xroar_unix_INT_OBJS)

############################################################################
# Optional extras (most only apply to Unix-style build)

xroar_opt_OBJS =
xroar_opt_objc_OBJS =
xroar_opt_INT_OBJS =

opt_gtk2_OBJS = gtk2/ui_gtk2.o gtk2/filereq_gtk2.o gtk2/keyboard_gtk2.o
CLEAN += $(opt_gtk2_OBJS)
ifeq ($(opt_gtk2),yes)
	xroar_opt_OBJS += $(opt_gtk2_OBJS)
	xroar_opt_CFLAGS += $(opt_gtk2_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_gtk2_LDFLAGS)
gtk2/ui_gtk2.o: gtk2/top_window_glade.h gtk2/tapecontrol_glade.h \
	gtk2/drivecontrol_glade.h
keyboard_gtk2.o: $(SRCROOT)/gtk2/keyboard_gtk2_mappings.c
$(opt_gtk2_OBJS): | gtk2
gtk2:
	mkdir -p gtk2
endif

opt_gtkgl_OBJS = gtk2/vo_gtkgl.o
CLEAN += $(opt_gtkgl_OBJS)
ifeq ($(opt_gtkgl),yes)
	xroar_opt_OBJS += $(opt_gtkgl_OBJS)
	xroar_opt_CFLAGS += $(opt_gtkgl_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_gtkgl_LDFLAGS)
vo_gtkgl.o: vdg_bitmaps.h
$(opt_gtkgl_OBJS): | gtk2
endif

opt_sdl_OBJS = sdl/ui_sdl.o sdl/vo_sdl.o sdl/vo_sdlyuv.o sdl/ao_sdl.o \
		sdl/keyboard_sdl.o sdl/joystick_sdl.o
CLEAN += $(opt_sdl_OBJS)
ifeq ($(opt_sdl),yes)
	xroar_opt_OBJS += $(opt_sdl_OBJS)
	xroar_opt_CFLAGS += $(opt_sdl_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_sdl_LDFLAGS)
vo_sdl.o: vdg_bitmaps.h
vo_sdlyuv.o: vdg_bitmaps.h
keyboard_sdl.o: $(SRCROOT)/sdl/keyboard_sdl_mappings.c
$(opt_sdl_OBJS): | sdl
sdl:
	mkdir -p sdl
endif

opt_sdlgl_OBJS = sdl/vo_sdlgl.o
CLEAN += $(opt_sdlgl_OBJS)
ifeq ($(opt_sdlgl),yes)
	xroar_opt_OBJS += $(opt_sdlgl_OBJS)
	xroar_opt_CFLAGS += $(opt_sdlgl_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_sdlgl_LDFLAGS)
vo_sdlgl.o: vdg_bitmaps.h
$(opt_sdlgl_OBJS): | sdl
endif

opt_curses_OBJS = curses/ui_curses.o curses/vo_curses.o curses/keyboard_curses.o
CLEAN += $(opt_curses_OBJS)
ifeq ($(opt_curses),yes)
	xroar_opt_OBJS += $(opt_curses_OBJS)
	xroar_opt_CFLAGS += $(opt_curses_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_curses_LDFLAGS)
$(opt_curses_OBJS): | curses
curses:
	mkdir -p curses
endif

opt_gtk1_OBJS = gtk1/filereq_gtk1.o
CLEAN += $(opt_gtk1_OBJS)
ifeq ($(opt_gtk1),yes)
	xroar_opt_OBJS += $(opt_gtk1_OBJS)
	xroar_opt_CFLAGS += $(opt_gtk1_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_gtk1_LDFLAGS)
$(opt_gtk1_OBJS): | gtk1
gtk1:
	mkdir -p gtk1
endif

opt_cli_OBJS = filereq_cli.o
CLEAN += $(opt_cli_OBJS)
ifeq ($(opt_cli),yes)
	xroar_opt_OBJS += $(opt_cli_OBJS)
	xroar_opt_CFLAGS += $(opt_cli_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_cli_LDFLAGS)
endif

opt_cocoa_objc_OBJS = macosx/ui_macosx.o macosx/filereq_cocoa.o
CLEAN += $(opt_cocoa_OBJS) $(opt_cocoa_objc_OBJS)
ifeq ($(opt_cocoa),yes)
	xroar_opt_objc_OBJS += $(opt_cocoa_objc_OBJS)
	xroar_opt_CFLAGS += $(opt_cocoa_CFLAGS)
	xroar_opt_OBJCFLAGS += $(opt_cocoa_OBJCFLAGS)
	xroar_opt_LDFLAGS += $(opt_cocoa_LDFLAGS)
$(opt_cocoa_objc_OBJS): | macosx
macosx:
	mkdir -p macosx
endif

opt_alsa_OBJS = alsa/ao_alsa.o
CLEAN += $(opt_alsa_OBJS)
ifeq ($(opt_alsa),yes)
	xroar_opt_OBJS += $(opt_alsa_OBJS)
	xroar_opt_CFLAGS += $(opt_alsa_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_alsa_LDFLAGS)
$(opt_alsa_OBJS): | alsa
alsa:
	mkdir -p alsa
endif

opt_oss_OBJS = oss/ao_oss.o
CLEAN += $(opt_oss_OBJS)
ifeq ($(opt_oss),yes)
	xroar_opt_OBJS += $(opt_oss_OBJS)
	xroar_opt_CFLAGS += $(opt_oss_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_oss_LDFLAGS)
$(opt_oss_OBJS): | oss
oss:
	mkdir -p oss
endif

opt_pulse_OBJS = pulseaudio/ao_pulse.o
CLEAN += $(opt_pulse_OBJS)
ifeq ($(opt_pulse),yes)
	xroar_opt_OBJS += $(opt_pulse_OBJS)
	xroar_opt_CFLAGS += $(opt_pulse_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_pulse_LDFLAGS)
$(opt_pulse_OBJS): | pulseaudio
pulseaudio:
	mkdir -p pulseaudio
endif

opt_sunaudio_OBJS = sunos/ao_sun.o
CLEAN += $(opt_sunaudio_OBJS)
ifeq ($(opt_sunaudio),yes)
	xroar_opt_OBJS += $(opt_sunaudio_OBJS)
	xroar_opt_CFLAGS += $(opt_sunaudio_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_sunaudio_LDFLAGS)
$(opt_sunaudio_OBJS): | sunos
sunos:
	mkdir -p sunos
endif

opt_coreaudio_OBJS = macosx/ao_macosx.o
CLEAN += $(opt_coreaudio_OBJS)
ifeq ($(opt_coreaudio),yes)
	xroar_opt_OBJS += $(opt_coreaudio_OBJS)
	xroar_opt_CFLAGS += $(opt_coreaudio_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_coreaudio_LDFLAGS)
$(opt_coreaudio_OBJS): | macosx
endif

opt_jack_OBJS = jack/ao_jack.o
CLEAN += $(opt_jack_OBJS)
ifeq ($(opt_jack),yes)
	xroar_opt_OBJS += $(opt_jack_OBJS)
	xroar_opt_CFLAGS += $(opt_jack_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_jack_LDFLAGS)
$(opt_jack_OBJS): | jack
jack:
	mkdir -p jack
endif

opt_sndfile_OBJS = tape_sndfile.o
CLEAN += $(opt_sndfile_OBJS)
ifeq ($(opt_sndfile),yes)
	xroar_opt_OBJS += $(opt_sndfile_OBJS)
	xroar_opt_CFLAGS += $(opt_sndfile_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_sndfile_LDFLAGS)
endif

opt_nullaudio_OBJS = ao_null.o
CLEAN += $(opt_nullaudio_OBJS)
ifeq ($(opt_nullaudio),yes)
	xroar_opt_OBJS += $(opt_nullaudio_OBJS)
	xroar_opt_CFLAGS += $(opt_nullaudio_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_nullaudio_LDFLAGS)
endif

opt_linux_joystick_OBJS = linux/joystick_linux.o
CLEAN += $(opt_linux_joystick_OBJS)
ifeq ($(opt_linux_joystick),yes)
	xroar_opt_OBJS += $(opt_linux_joystick_OBJS)
	xroar_opt_CFLAGS += $(opt_linux_joystick_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_linux_joystick_LDFLAGS)
$(opt_linux_joystick_OBJS): | linux
linux:
	mkdir -p linux
endif

opt_mingw_OBJS = windows32/common_windows32.o windows32/filereq_windows32.o
CLEAN += $(opt_mingw_OBJS)
ifeq ($(opt_mingw),yes)
	xroar_opt_OBJS += $(opt_mingw_OBJS)
	xroar_opt_CFLAGS += $(opt_mingw_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_mingw_LDFLAGS)
$(opt_mingw_OBJS): | windows32
windows32:
	mkdir -p windows32
endif

opt_trace_OBJS = m6809_trace.o
CLEAN += $(opt_trace_OBJS)
ifeq ($(opt_trace),yes)
	xroar_opt_OBJS += $(opt_trace_OBJS)
	xroar_opt_CFLAGS += $(opt_trace_CFLAGS)
	xroar_opt_LDFLAGS += $(opt_trace_LDFLAGS)
endif

############################################################################
# Build rules

# GP32 rules
include gp32/makefile_gp32.mk

# Nintendo DS rules
include nds/makefile_nds.mk

# Unix rules (default)
ifeq ($(BUILD_STYLE),)

ifeq ($(opt_mingw),yes)
ROMPATH = \":~/Local\ Settings/Application\ Data/XRoar/roms:~/Application\ Data/XRoar/roms\"
CONFPATH = \":~/Local\ Settings/Application\ Data/XRoar:~/Application\ Data/XRoar\"
endif

ifeq ($(opt_coreaudio),yes)
ROMPATH = \"~/Library/XRoar/Roms:~/.xroar/roms:$(datadir)/xroar/roms:\"
CONFPATH = \"~/Library/XRoar:~/.xroar:$(sysconfdir):$(datadir)/xroar\"
endif

ifndef ROMPATH
ROMPATH = \"~/.xroar/roms:$(datadir)/xroar/roms:\"
CONFPATH = \"~/.xroar:$(sysconfdir):$(datadir)/xroar\"
endif

xroar_unix_CFLAGS = $(CFLAGS) $(CPPFLAGS) -D_XOPEN_SOURCE=700 \
	$(xroar_opt_CFLAGS) \
	-I$(CURDIR) -I$(SRCROOT) $(WARN) \
        -DVERSION=\"$(VERSION)\" \
        -DROMPATH=$(ROMPATH) -DCONFPATH=$(CONFPATH)
xroar_unix_OBJCFLAGS = $(OBJCFLAGS) $(CPPFLAGS) \
	$(xroar_opt_CFLAGS) $(xroar_opt_OBJCFLAGS) \
	-I$(CURDIR) -I$(SRCROOT) $(WARN) \
        -DVERSION=\"$(VERSION)\" \
        -DROMPATH=$(ROMPATH) -DCONFPATH=$(CONFPATH)
xroar_unix_LDFLAGS = $(LDFLAGS) $(LDLIBS) $(xroar_opt_LDFLAGS)

xroar_unix_ALL_OBJS = $(xroar_common_OBJS) $(xroar_common_INT_OBJS) \
	$(xroar_unix_OBJS) $(xroar_unix_INT_OBJS) \
	$(xroar_opt_OBJS) $(xroar_opt_INT_OBJS) \
	$(xroar_common_objc_OBJS) $(xroar_unix_objc_OBJS) \
	$(xroar_opt_objc_OBJS)

$(xroar_unix_ALL_OBJS): $(CONFIG_FILES)

$(xroar_common_OBJS) $(xroar_unix_OBJS) $(xroar_opt_OBJS): %.o: $(SRCROOT)/%.c
	$(call do_cc,$@,$(xroar_unix_CFLAGS) -c $<)

$(xroar_common_objc_OBJS) $(xroar_unix_objc_OBJS) $(xroar_opt_objc_OBJS): %.o: $(SRCROOT)/%.m
	$(call do_objc,$@,$(xroar_unix_OBJCFLAGS) -c $<)

$(xroar_common_INT_OBJS) $(xroar_unix_INT_OBJS) $(xroar_opt_INT_OBJS): %.o: ./%.c
	$(call do_cc,$@,$(xroar_unix_CFLAGS) -c $<)

xroar$(EXEEXT): $(xroar_unix_ALL_OBJS)
	$(call do_cc,$@,$(xroar_unix_ALL_OBJS) $(xroar_unix_LDFLAGS))

.PHONY: build-bin
build-bin: xroar$(EXEEXT)

endif

CLEAN += xroar xroar.exe

############################################################################
# Documentation build rules

doc:
	mkdir -p doc

doc/xroar.info: $(SRCROOT)/doc/xroar.texi | doc
	$(call do_makeinfo,$@,-D "VERSION $(VERSION)" $<)

doc/xroar.pdf: $(SRCROOT)/doc/xroar.texi | doc
	$(call do_texi2pdf,$@,-q -t "@set VERSION $(VERSION)" --build=clean $<)

doc/xroar.html: $(SRCROOT)/doc/xroar.texi | doc
	$(call do_makeinfo,$@,--html --no-headers --no-split -D "VERSION $(VERSION)" $<)

doc/xroar.txt: $(SRCROOT)/doc/xroar.texi | doc
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
	$(INSTALL_PROGRAM) xroar$(EXEEXT) $(DESTDIR)$(bindir)

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

tools:
	mkdir -p tools

.SECONDARY: tools/font2c
tools/font2c: $(SRCROOT)/tools/font2c.c | tools
	$(call do_build_cc,$@,$(opt_build_sdl_CFLAGS) $< $(opt_build_sdl_LDFLAGS) $(opt_build_sdl_image_LDFLAGS))

CLEAN += tools/font2c

#

vdg_bitmaps.h: tools/font2c | $(SRCROOT)/vdgfont.png
	tools/font2c --header --array vdg_alpha --type "unsigned int" --vdg $(SRCROOT)/vdgfont.png > $@

vdg_bitmaps.c: tools/font2c | $(SRCROOT)/vdgfont.png
	tools/font2c --array vdg_alpha --type "unsigned int" --vdg $(SRCROOT)/vdgfont.png > $@

#

gtk2/%_glade.h: $(SRCROOT)/gtk2/%.glade
	echo "static const gchar *$(@:%.h=%) =" | sed 's/\*.*\//\*/'> $@
	sed 's/"/'\''/g;s/^\( *\)/\1"/;s/$$/"/;' $< >> $@
	echo ";" >> $@

############################################################################
# Distribution creation

.PHONY: dist
dist:
	git archive --format=tar --output=../$(DISTNAME).tar --prefix=$(DISTNAME)/ HEAD
	gzip -f9 ../$(DISTNAME).tar

.PHONY: dist-windows32
dist-windows32: all doc/xroar.pdf
	mkdir $(DISTNAME)-windows32
	cp $(SRCROOT)/COPYING.GPL $(SRCROOT)/ChangeLog $(SRCROOT)/README doc/xroar.pdf xroar.exe /usr/local/$(TARGET_ARCH)/bin/SDL.dll /usr/local/$(TARGET_ARCH)/bin/libsndfile-1.dll $(DISTNAME)-windows32/
	cp $(SRCROOT)/COPYING.LGPL-2.1 $(DISTNAME)-windows32/COPYING.LGPL-2.1
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

.PHONY: distclean
distclean: clean
	rm -f config.h config.mak config.log

.PHONY: depend
depend:
	@touch .deps.mak
	makedepend -f .deps.mak -Y `git ls-files | sort | grep '\.c'` 2> /dev/null

-include .deps.mak
