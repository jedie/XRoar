# Makefile for XRoar.
# Needs you to run './configure' first.  './configure --help' for help.

include config.mak

VERSION := 0.18

.PHONY: all usage gp32 windows32

# TARGET is the output from gcc, not necessarily the final binary
TARGET = xroar$(EXEEXT)

# Make default target depend on appropriate final binary
ifeq ($(CONFIG_GP32),yes)
all: xroar.fxe
else
all: $(TARGET)
endif

usage:
	@echo
	@echo "Usage: \"make system-type\", where system-type is one of:"
	@echo
	@echo " gp32 linux macosx solaris windows32"
	@echo
	@echo "You can append options to determine which tools are used to compile."
	@echo "e.g., \"make gp32 TARGET_ARCH=arm-elf\""
	@echo

gp32:
	./configure --target=arm-elf
	$(MAKE) all

windows32:
	./configure --target=i586-mingw32
	$(MAKE) all

VPATH = $(SRCROOT)

WARN = -Wall -W -Wstrict-prototypes -Wpointer-arith -Wcast-align \
	-Wcast-qual -Wshadow -Waggregate-return -Wnested-externs -Winline \
	-Wwrite-strings -Wundef -Wsign-compare -Wmissing-prototypes \
	-Wredundant-decls

ifeq ($(CONFIG_MINGW),yes)
ROMPATH = \".\"
else
ifeq ($(CONFIG_GP32),yes)
ROMPATH = \"gp:/gpmm/dragon\"
else
ROMPATH = \".\",\"~/.xroar/roms\",\"$(prefix)/share/xroar/roms\",\"~/Library/XRoar/Roms\"
endif
endif

CFLAGS += -I$(CURDIR) -I$(SRCROOT) $(WARN) -g -DVERSION=\"$(VERSION)\" -DROMPATH=$(ROMPATH)

############################################################################
# Base object files required by all builds

OBJS := cart.o crc16.o events.o hexs19.o joystick.o keyboard.o m6809.o \
		machine.o module.o pia.o sam.o snapshot.o sound_null.o tape.o \
		vdg.o vdisk.o vdrive.o wd2797.o xroar.o

ALL_OBJS := $(OBJS)

############################################################################
# Optional extras

OBJS_SDL = ui_sdl.o video_sdl.o video_sdlyuv.o sound_sdl.o keyboard_sdl.o \
		joystick_sdl.o
ALL_OBJS += $(OBJS_SDL)
ifeq ($(CONFIG_SDL),yes)
	OBJS += $(OBJS_SDL)
endif

OBJS_SDLGL = video_sdlgl.o
ALL_OBJS += $(OBJS_SDLGL)
ifeq ($(CONFIG_SDLGL),yes)
	OBJS += $(OBJS_SDLGL)
endif

OBJS_GTK = filereq_gtk.o
ALL_OBJS += $(OBJS_GTK)
ifeq ($(CONFIG_GTK),yes)
	OBJS += $(OBJS_GTK)
endif

OBJS_CARBON = filereq_carbon.o
ALL_OBJS += $(OBJS_CARBON)
ifeq ($(CONFIG_CARBON),yes)
	OBJS += $(OBJS_CARBON)
endif

OBJS_OSS = sound_oss.o
ALL_OBJS += $(OBJS_OSS)
ifeq ($(CONFIG_OSS),yes)
	OBJS += $(OBJS_OSS)
endif

OBJS_SUN = sound_sun.o
ALL_OBJS += $(OBJS_SUN)
ifeq ($(CONFIG_SUN),yes)
	OBJS += $(OBJS_SUN)
endif

OBJS_COREAUDIO = sound_macosx.o
ALL_OBJS += $(OBJS_COREAUDIO)
ifeq ($(CONFIG_COREAUDIO),yes)
	OBJS += $(OBJS_COREAUDIO)
endif

OBJS_JACK = sound_jack.o
ALL_OBJS += $(OBJS_JACK)
ifeq ($(CONFIG_JACK),yes)
	OBJS += $(OBJS_JACK)
endif

OBJS_WINDOWS32 = common_windows32.o filereq_windows32.o
ALL_OBJS += $(OBJS_WINDOWS32)
ifeq ($(CONFIG_MINGW),yes)
	OBJS += $(OBJS_WINDOWS32)
endif

OBJS_TRACE = m6809_dasm.o
ALL_OBJS += $(OBJS_TRACE)
ifeq ($(CONFIG_TRACE),yes)
	OBJS += $(OBJS_TRACE)
endif

OBJS_GP32 = fs_gp32.o main_gp32.o keyboard_gp32.o sound_gp32.o \
		ui_gp32.o video_gp32.o gp32/gpstart.o gp32/udaiis.o \
		gp32/gpgfx.o gp32/gpsound.o gp32/gpkeypad.o gp32/gpchatboard.o \
		gp32/cmode_bin.o gp32/copyright.o gp32/kbd_graphics.o
ALL_OBJS += $(OBJS_GP32)

OBJS_UNIX = fs_unix.o main_unix.o
ALL_OBJS += $(OBJS_UNIX)

ifeq ($(CONFIG_GP32),yes)
	OBJS += $(OBJS_GP32)
else
	OBJS += $(OBJS_UNIX)
endif

############################################################################
# Build rules

$(OBJS): config.h config.mak

$(TARGET): $(CRT0) $(OBJS)
	$(CC) $(CFLAGS) $(CRT0) $(OBJS) -o $@ $(LDFLAGS)

############################################################################
# Targets where output of gcc is not the end result

ifeq ($(CONFIG_GP32),yes)
xroar.fxe: xroar.elf
	$(OBJCOPY) -O binary xroar.elf xroar.bin
	b2fxec -t "XRoar $(VERSION)" -a "Ciaran Anscomb" -b $(SRCROOT)/gp32/icon.bmp -f xroar.bin $@
endif

############################################################################
# Generated dependencies and the tools that generate them

$(SRCROOT)/video_gp32.c: gp32/vdg_bitmaps_gp32.c

gp32/vdg_bitmaps_gp32.c: tools/prerender vdg_bitmaps.c
	tools/prerender > $@

tools/prerender: tools/prerender.c
	mkdir -p tools
	$(BUILD_CC) -o $@ $<

gp32/copyright.c: tools/img2c gp32/copyright.png
	tools/img2c copyright_bin $(SRCROOT)/gp32/copyright.png > $@

gp32/cmode_bin.c: tools/img2c gp32/cmode_kbd.png gp32/cmode_cur.png gp32/cmode_joyr.png gp32/cmode_joyl.png
	tools/img2c cmode_bin $(SRCROOT)/gp32/cmode_kbd.png $(SRCROOT)/gp32/cmode_cur.png $(SRCROOT)/gp32/cmode_joyr.png $(SRCROOT)/gp32/cmode_joyl.png > $@

gp32/kbd_graphics.c: tools/img2c gp32/kbd.png gp32/kbd_shift.png
	tools/img2c kbd_bin $(SRCROOT)/gp32/kbd.png $(SRCROOT)/gp32/kbd_shift.png > $@

tools/img2c: tools/img2c.c
	mkdir -p tools
	$(BUILD_CC) $(BUILD_SDL_CFLAGS) -o $@ $< $(BUILD_SDL_LDFLAGS) -lSDL_image

############################################################################
# Distribution creation and cleanup

.PHONY: clean distclean dist dist-gp32 dist-windows32 dist-macos dist-macosx

CLEAN_FILES = $(CRT0) $(ALL_OBJS) tools/img2c tools/prerender \
	gp32/copyright.c gp32/cmode_bin.c gp32/kbd_graphics.c \
	gp32/vdg_bitmaps_gp32.c xroar.bin xroar.fxe xroar.elf \
	xroar

clean:
	rm -f $(CLEAN_FILES)

distclean: clean
	rm -f config.h config.mak config.log

DISTNAME = xroar-$(VERSION)

dist:
	darcs dist --dist-name $(DISTNAME)
	mv $(DISTNAME).tar.gz ..

dist-gp32: gp32
	mkdir $(DISTNAME)-gp32
	cp COPYING.GPL ChangeLog README xroar.fxe $(DISTNAME)-gp32/
	rm -f ../$(DISTNAME)-gp32.zip
	zip -r ../$(DISTNAME)-gp32.zip $(DISTNAME)-gp32
	rm -rf $(DISTNAME)-gp32/

dist-windows32: windows32
	mkdir $(DISTNAME)-windows32
	cp COPYING.GPL ChangeLog README xroar.exe /usr/local/$(TARGET_ARCH)/bin/SDL.dll /usr/local/$(TARGET_ARCH)/bin/libsndfile-1.dll $(DISTNAME)-windows32/
	cp COPYING.LGPL-2.1 $(DISTNAME)-windows32/COPYING.LGPL-2.1
	$(TOOL_PREFIX)strip $(DISTNAME)-windows32/xroar.exe
	$(TOOL_PREFIX)strip $(DISTNAME)-windows32/SDL.dll
	$(TOOL_PREFIX)strip $(DISTNAME)-windows32/libsndfile-1.dll
	rm -f ../$(DISTNAME)-windows32.zip
	zip -r ../$(DISTNAME)-windows32.zip $(DISTNAME)-windows32
	rm -rf $(DISTNAME)-windows32/

dist-macosx dist-macos: macosx
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
	cp README COPYING.GPL ChangeLog XRoar-$(VERSION)/
	cp COPYING.LGPL-2.1 XRoar-$(VERSION)/COPYING.LGPL-2.1
	chmod -R o+rX,g+rX XRoar-$(VERSION)/
	hdiutil create -srcfolder XRoar-$(VERSION) -uid 99 -gid 99 ../XRoar-$(VERSION).dmg
	rm -rf XRoar-$(VERSION)/

.PHONY: depend
depend:
	makedepend -Y. `darcs query manifest | sort | grep '\.c'` 2> /dev/null

# DO NOT DELETE

