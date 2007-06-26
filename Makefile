# Makefile for XRoar.
# Needs you to run './configure' first.  './configure --help' for help.

-include config.mak

VERSION := 0.20

.PHONY: all usage gp32 windows32

# TARGET is the output from gcc, not necessarily the final binary
TARGET = xroar$(EXEEXT)

# Make default target depend on appropriate final binary
ifeq ($(CONFIG_GP32),yes)
all: xroar.fxe
else
ifeq ($(CONFIG_NDS),yes)
all: xroar.nds
else
all: $(TARGET)
endif
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

nds:
	./configure --target=arm-nds-eabi
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
ifeq ($(CONFIG_NDS),yes)
ROMPATH = \"fat:/dragon\"
else
ROMPATH = \".\",\"~/.xroar/roms\",\"~/Library/XRoar/Roms\",\"$(prefix)/share/xroar/roms\"
endif
endif
endif

CFLAGS += -I$(CURDIR) -I$(SRCROOT) $(WARN) -g -DVERSION=\"$(VERSION)\" -DROMPATH=$(ROMPATH)

############################################################################
# Build rules for ARM7

NDS_ARM7_CFLAGS = -O3 -pipe -mcpu=arm7tdmi -mtune=arm7tdmi -fomit-frame-pointer -ffast-math -mthumb -mthumb-interwork -DARM7 -I$(CURDIR) -I$(SRCROOT) $(WARN) -g -DVERSION=\"$(VERSION)\" -DROMPATH=$(ROMPATH)
NDS_ARM7_LDFLAGS = -specs=ds_arm7.specs -lnds7
xroar.arm7: CFLAGS = $(NDS_ARM7_CFLAGS)
xroar.arm7: LDFLAGS = $(NDS_ARM7_LDFLAGS)

############################################################################
# Base object files required by all builds

OBJS := cart.o crc16.o events.o hexs19.o joystick.o keyboard.o m6809.o \
		machine.o module.o pia.o sam.o snapshot.o sound_null.o tape.o \
		vdg.o vdg_bitmaps.o vdisk.o vdrive.o dragondos.o rsdos.o \
		deltados.o wd279x.o xroar.o

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

OBJS_CLI = filereq_cli.o
ALL_OBJS += $(OBJS_CLI)
ifeq ($(CONFIG_CLI),yes)
	OBJS += $(OBJS_CLI)
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

OBJS_NDS9 = fs_unix.o main_nds9.o ui_nds.o video_nds.o \
		nds/ndsgfx.o \
		nds/ndsui.o nds/ndsui_button.o nds/ndsui_filelist.o \
		nds/ndsui_keyboard.o nds/ndsui_scrollbar.o \
		nds/nds_font8x8.o nds/kbd_graphics.o
OBJS_NDS7 = main_nds7.o
ALL_OBJS += $(OBJS_NDS9) $(OBJS_NDS7)

OBJS_UNIX = fs_unix.o main_unix.o
ALL_OBJS += $(OBJS_UNIX)

ifeq ($(CONFIG_GP32),yes)
	OBJS += $(OBJS_GP32)
else
ifeq ($(CONFIG_NDS),yes)
	#OBJS += $(OBJS_NDS)
else
	OBJS += $(OBJS_UNIX)
endif
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

ifeq ($(CONFIG_NDS),yes)
xroar.arm7: $(OBJS_NDS7)
	$(CC) $(CFLAGS) $(OBJS_NDS7) -o $@ $(LDFLAGS)

xroar.arm7.bin: xroar.arm7
	$(OBJCOPY) -O binary $< $@

xroar.arm9: $(OBJS_NDS9) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS_NDS9) $(OBJS) -o $@ $(LDFLAGS)

xroar.arm9.bin: xroar.arm9
	$(OBJCOPY) -O binary $< $@

xroar.nds: xroar.arm7.bin xroar.arm9.bin
	ndstool -c $@ -7 xroar.arm7.bin -9 xroar.arm9.bin
	dsbuild $@
endif

############################################################################
# Generated dependencies and the tools that generate them

tools/font2c: tools/font2c.c
	mkdir -p tools
	$(BUILD_CC) $(BUILD_SDL_CFLAGS) -o $@ $< $(BUILD_SDL_LDFLAGS) -lSDL_image

$(SRCROOT)/vdg_bitmaps.c: tools/font2c $(SRCROOT)/vdgfont.png
	tools/font2c --array vdg_alpha --type "unsigned int" --vdg $(SRCROOT)/vdgfont.png > $@

$(SRCROOT)/video_gp32.c: gp32/vdg_bitmaps_gp32.c

gp32/vdg_bitmaps_gp32.c: tools/prerender
	tools/prerender > $@

tools/prerender: tools/prerender.c $(SRCROOT)/vdg_bitmaps.c
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

nds/kbd_graphics.c: tools/img2c_nds nds/kbd.png nds/kbd_shift.png
	tools/img2c_nds kbd_bin $(SRCROOT)/nds/kbd.png $(SRCROOT)/nds/kbd_shift.png > $@

nds/nds_font8x8.c: tools/font2c vdgfont.png
	tools/font2c --array nds_font8x8 --type "unsigned char" $(SRCROOT)/vdgfont.png > $@

tools/img2c_nds: tools/img2c_nds.c
	mkdir -p tools
	$(BUILD_CC) $(BUILD_SDL_CFLAGS) -o $@ $< $(BUILD_SDL_LDFLAGS) -lSDL_image

############################################################################
# Distribution creation and cleanup

.PHONY: clean distclean dist dist-gp32 dist-windows32 dist-macos dist-macosx

CLEAN_FILES = $(CRT0) $(ALL_OBJS) tools/img2c tools/img2c_nds tools/prerender \
	tools/font2c vdg_bitmaps.c \
	gp32/copyright.c gp32/cmode_bin.c gp32/kbd_graphics.c \
	gp32/vdg_bitmaps_gp32.c xroar.bin xroar.fxe xroar.elf \
	nds/kbd_graphics.c nds/nds_font8x8.c xroar.nds xroar.ds.gba \
	xroar.arm7 xroar.arm9 xroar.arm7.bin xroar.arm9.bin xroar xroar.exe

clean:
	rm -f $(CLEAN_FILES)

distclean: clean
	rm -f config.h config.mak config.log

DISTNAME = xroar-$(VERSION)

dist:
	darcs dist --dist-name $(DISTNAME)
	gzip -dc $(DISTNAME).tar.gz | tar xf -
	chmod u+x $(DISTNAME)/configure
	chmod -R g+rX,o+rX $(DISTNAME)
	rm -f $(DISTNAME).tar.gz
	tar cf - $(DISTNAME) | gzip -c > $(DISTNAME).tar.gz
	rm -rf $(DISTNAME)
	mv $(DISTNAME).tar.gz ..

dist-gp32: all
	mkdir $(DISTNAME)-gp32
	cp COPYING.GPL ChangeLog README xroar.fxe $(DISTNAME)-gp32/
	rm -f ../$(DISTNAME)-gp32.zip
	zip -r ../$(DISTNAME)-gp32.zip $(DISTNAME)-gp32
	rm -rf $(DISTNAME)-gp32/

dist-windows32: all
	mkdir $(DISTNAME)-windows32
	cp COPYING.GPL ChangeLog README xroar.exe /usr/local/$(TARGET_ARCH)/bin/SDL.dll /usr/local/$(TARGET_ARCH)/bin/libsndfile-1.dll $(DISTNAME)-windows32/
	cp COPYING.LGPL-2.1 $(DISTNAME)-windows32/COPYING.LGPL-2.1
	$(TOOL_PREFIX)strip $(DISTNAME)-windows32/xroar.exe
	$(TOOL_PREFIX)strip $(DISTNAME)-windows32/SDL.dll
	$(TOOL_PREFIX)strip $(DISTNAME)-windows32/libsndfile-1.dll
	rm -f ../$(DISTNAME)-windows32.zip
	zip -r ../$(DISTNAME)-windows32.zip $(DISTNAME)-windows32
	rm -rf $(DISTNAME)-windows32/

dist-macosx dist-macos: all
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
	makedepend -Y `darcs query manifest | sort | grep '\.c'` 2> /dev/null

# DO NOT DELETE

./cart.o: types.h config.h events.h fs.h logging.h machine.h pia.h joystick.h
./cart.o: keyboard.h m6809.h module.h tape.h vdg.h xroar.h cart.h
./common_windows32.o: common_windows32.h
./crc16.o: types.h config.h crc16.h
./deltados.o: types.h config.h deltados.h logging.h vdrive.h vdisk.h wd279x.h
./deltados.o: xroar.h
./dragondos.o: types.h config.h dragondos.h events.h logging.h m6809.h
./dragondos.o: machine.h pia.h joystick.h keyboard.h module.h tape.h vdg.h
./dragondos.o: vdrive.h vdisk.h wd279x.h xroar.h
./events.o: types.h config.h logging.h events.h
./filereq_carbon.o: types.h config.h logging.h module.h
./filereq_cli.o: types.h config.h logging.h fs.h module.h
./filereq_gtk.o: types.h config.h logging.h fs.h module.h
./filereq_windows32.o: types.h config.h logging.h fs.h module.h
./filereq_windows32.o: common_windows32.h
./fs_gp32.o: fs.h types.h config.h
./fs_nds.o: types.h config.h fs.h
./fs_unix.o: types.h config.h fs.h
./gp32/gpchatboard.o: ./gp32/gp32.h ./gp32/keydefines.h ./types.h config.h
./gp32/gpchatboard.o: ./gp32/gpchatboard.h
./gp32/gpgfx.o: ./types.h config.h ./gp32/gpgfx.h
./gp32/gpkeypad.o: ./gp32/gp32.h ./gp32/gpkeypad.h
./gp32/gpsound.o: ./gp32/gp32.h ./gp32/udaiis.h ./types.h config.h
./gp32/gpsound.o: ./gp32/gpsound.h
./gp32/udaiis.o: ./types.h config.h ./gp32/udaiis.h ./gp32/gp32.h
./hexs19.o: types.h config.h sam.h logging.h fs.h m6809.h hexs19.h machine.h
./joystick.o: types.h config.h logging.h module.h pia.h joystick.h keyboard.h
./joystick.o: m6809.h tape.h vdg.h
./joystick_sdl.o: types.h config.h events.h joystick.h logging.h machine.h
./joystick_sdl.o: module.h pia.h keyboard.h m6809.h tape.h vdg.h
./keyboard.o: types.h config.h events.h keyboard.h logging.h machine.h pia.h
./keyboard.o: joystick.h m6809.h module.h tape.h vdg.h
./keyboard_gp32.o: gp32/gp32.h gp32/gpgfx.h ./types.h config.h
./keyboard_gp32.o: gp32/gpkeypad.h gp32/gpchatboard.h types.h logging.h
./keyboard_gp32.o: events.h joystick.h keyboard.h machine.h module.h pia.h
./keyboard_gp32.o: m6809.h tape.h vdg.h snapshot.h xroar.h
./keyboard_sdl.o: types.h config.h logging.h cart.h events.h hexs19.h
./keyboard_sdl.o: joystick.h keyboard.h machine.h module.h pia.h m6809.h
./keyboard_sdl.o: tape.h vdg.h snapshot.h vdisk.h vdrive.h xroar.h
./keyboard_sdl.o: keyboard_sdl_mappings.c
./m6809.o: types.h config.h logging.h m6809.h m6809_dasm.h machine.h sam.h
./m6809.o: xroar.h
./m6809_dasm.o: types.h config.h logging.h m6809_dasm.h
./machine.o: types.h config.h cart.h deltados.h dragondos.h fs.h keyboard.h
./machine.o: logging.h m6809.h machine.h pia.h joystick.h module.h tape.h
./machine.o: vdg.h rsdos.h sam.h vdrive.h vdisk.h wd279x.h xroar.h
./main_gp32.o: gp32/gpgfx.h ./types.h config.h types.h xroar.h
./main_nds7.o: types.h config.h
./main_nds9.o: config.h xroar.h types.h logging.h
./main_unix.o: config.h xroar.h types.h logging.h
./module.o: types.h config.h logging.h module.h
./nds/ndsgfx.o: ./types.h config.h ./nds/ndsgfx.h
./nds/ndsui.o: types.h config.h logging.h module.h nds/ndsui.h nds/ndsgfx.h
./nds/ndsui.o: ./types.h
./nds/ndsui_button.o: types.h config.h logging.h module.h nds/ndsgfx.h
./nds/ndsui_button.o: ./types.h nds/ndsui.h nds/ndsui_button.h
./nds/ndsui_filelist.o: types.h config.h logging.h module.h nds/ndsgfx.h
./nds/ndsui_filelist.o: ./types.h nds/ndsui.h nds/ndsui_filelist.h
./nds/ndsui_keyboard.o: types.h config.h logging.h module.h nds/ndsgfx.h
./nds/ndsui_keyboard.o: ./types.h nds/ndsui.h nds/ndsui_keyboard.h
./nds/ndsui_scrollbar.o: types.h config.h logging.h module.h nds/ndsgfx.h
./nds/ndsui_scrollbar.o: ./types.h nds/ndsui.h nds/ndsui_scrollbar.h
./pia.o: types.h config.h logging.h m6809.h machine.h xroar.h pia.h
./pia.o: joystick.h keyboard.h module.h tape.h vdg.h
./rsdos.o: types.h config.h events.h logging.h m6809.h machine.h rsdos.h
./rsdos.o: vdrive.h vdisk.h wd279x.h xroar.h
./sam.o: types.h config.h deltados.h dragondos.h joystick.h keyboard.h
./sam.o: logging.h machine.h pia.h m6809.h module.h tape.h vdg.h rsdos.h
./sam.o: sam.h wd279x.h xroar.h
./snapshot.o: types.h config.h fs.h logging.h m6809.h machine.h pia.h
./snapshot.o: joystick.h keyboard.h module.h tape.h vdg.h sam.h snapshot.h
./snapshot.o: xroar.h
./sound_gp32.o: types.h config.h events.h machine.h module.h pia.h joystick.h
./sound_gp32.o: keyboard.h m6809.h tape.h vdg.h sound_gp32.h xroar.h
./sound_gp32.o: gp32/gpsound.h ./types.h gp32/gp32.h
./sound_jack.o: types.h config.h events.h logging.h machine.h pia.h
./sound_jack.o: joystick.h keyboard.h m6809.h module.h tape.h vdg.h xroar.h
./sound_macosx.o: types.h config.h logging.h events.h machine.h module.h
./sound_macosx.o: pia.h joystick.h keyboard.h m6809.h tape.h vdg.h xroar.h
./sound_null.o: config.h types.h logging.h events.h machine.h module.h
./sound_null.o: xroar.h
./sound_oss.o: types.h config.h events.h logging.h machine.h module.h pia.h
./sound_oss.o: joystick.h keyboard.h m6809.h tape.h vdg.h xroar.h
./sound_sdl.o: types.h config.h events.h logging.h machine.h module.h pia.h
./sound_sdl.o: joystick.h keyboard.h m6809.h tape.h vdg.h xroar.h
./sound_sun.o: types.h config.h events.h logging.h machine.h module.h pia.h
./sound_sun.o: joystick.h keyboard.h m6809.h tape.h vdg.h xroar.h
./tape.o: config.h types.h events.h fs.h logging.h machine.h pia.h joystick.h
./tape.o: keyboard.h m6809.h module.h tape.h vdg.h xroar.h
./ui_gp32.o: types.h config.h gp32/gp32.h gp32/gpgfx.h ./types.h
./ui_gp32.o: gp32/gpkeypad.h gp32/gpchatboard.h cart.h fs.h hexs19.h
./ui_gp32.o: keyboard.h machine.h module.h snapshot.h sound_gp32.h tape.h
./ui_gp32.o: vdg.h vdisk.h vdrive.h xroar.h
./ui_nds.o: types.h config.h events.h hexs19.h keyboard.h logging.h machine.h
./ui_nds.o: module.h snapshot.h tape.h vdisk.h vdrive.h xroar.h nds/ndsgfx.h
./ui_nds.o: ./types.h nds/ndsui.h nds/ndsui_button.h nds/ndsui_filelist.h
./ui_nds.o: nds/ndsui_keyboard.h nds/ndsui_scrollbar.h
./ui_sdl.o: types.h config.h logging.h module.h
./vdg.o: types.h config.h events.h logging.h m6809.h machine.h pia.h
./vdg.o: joystick.h keyboard.h module.h tape.h vdg.h sam.h xroar.h
./vdisk.o: types.h config.h crc16.h fs.h logging.h module.h vdisk.h xroar.h
./vdrive.o: types.h config.h crc16.h events.h logging.h machine.h vdisk.h
./vdrive.o: vdrive.h xroar.h
./video_generic_ops.o: machine.h types.h config.h
./video_gp32.o: gp32/gpgfx.h ./types.h config.h types.h module.h sam.h
./video_gp32.o: xroar.h
./video_nds.o: types.h config.h logging.h module.h sam.h vdg.h xroar.h
./video_sdl.o: types.h config.h logging.h module.h sam.h ui_sdl.h vdg.h
./video_sdl.o: xroar.h video_generic_ops.c machine.h
./video_sdlgl.o: types.h config.h logging.h module.h sam.h ui_sdl.h vdg.h
./video_sdlgl.o: xroar.h video_generic_ops.c machine.h
./video_sdlyuv.o: types.h config.h logging.h module.h sam.h ui_sdl.h vdg.h
./video_sdlyuv.o: xroar.h video_generic_ops.c machine.h
./wd279x.o: types.h config.h crc16.h events.h logging.h machine.h vdrive.h
./wd279x.o: vdisk.h wd279x.h xroar.h
./xroar.o: types.h config.h logging.h cart.h events.h fs.h m6809.h machine.h
./xroar.o: module.h pia.h joystick.h keyboard.h tape.h vdg.h sam.h snapshot.h
./xroar.o: xroar.h
