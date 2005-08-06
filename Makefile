### Install under this prefix:
prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
libdir = $(exec_prefix)/lib
mandir = $(prefix)/man
datadir = $(prefix)/share

.PHONY: usage gp32 linux linux-be macosx solaris solaris-le windows32

usage:
	@echo
	@echo "Usage: \"make system-type\", where system-type is one of:"
	@echo
	@echo " gp32 linux linux-be macosx solaris solaris-le windows32"
	@echo
	@echo "You can append options to determine which tools are used to compile."
	@echo "e.g., \"make gp32 TOOL_PREFIX=arm-elf-\""
	@echo
	@echo "-be and -le targets specify an endianness (big- or little-endian). "
	@echo

VERSION = 0.12

OPT = -O3

CFLAGS = $(OPT) -g -DVERSION=\"$(VERSION)\" -DROMPATH=\"$(ROMPATH)\"

SDL_CONFIG := sdl-config
CFLAGS_SDL = -DHAVE_SDL_VIDEO -DHAVE_SDL_AUDIO $(shell $(SDL_CONFIG) --cflags)
LDFLAGS_SDL = $(shell $(SDL_CONFIG) --libs)
LDFLAGS_SDLIMAGE = -lSDL_image
CFLAGS_GL = -DHAVE_SDLGL_VIDEO
LDFLAGS_GL = -lGL

CFLAGS_GTK = -DHAVE_GTK_UI $(shell gtk-config --cflags)
LDFLAGS_GTK = $(shell gtk-config --libs)

CFLAGS_CARBON = -DHAVE_CARBON_UI
LDFLAGS_CARBON = -framework Carbon

CFLAGS_OSS = -DHAVE_OSS_AUDIO
LDFLAGS_OSS =

CFLAGS_COREAUDIO = -DHAVE_MACOSX_AUDIO
LDFLAGS_COREAUDIO = -framework CoreAudio

CFLAGS_WRONGENDIAN = -DWRONG_ENDIAN

ROMPATH = :$(prefix)/share/xroar/roms

ifdef USE_JACK_AUDIO
	CFLAGS += -DHAVE_JACK_AUDIO
	LDFLAGS += -ljack -lpthread
endif
ifdef USE_CLI_UI
        CFLAGS += -DHAVE_CLI_UI
endif
ifdef TRACE
	CFLAGS += -DTRACE
endif

linux: CFLAGS += $(CFLAGS_SDL) $(CFLAGS_GL) $(CFLAGS_GTK) $(CFLAGS_OSS) \
	$(CFLAGS_WRONGENDIAN) -DHAVE_NULL_AUDIO
linux: LDFLAGS += $(LDFLAGS_SDL) $(LDFLAGS_GL) $(LDFLAGS_GTK) $(LDFLAGS_OSS)
linux: xroar

linux-be: CFLAGS_WRONGENDIAN :=
linux-be: linux

gp32: TOOL_PREFIX := arm-elf-
gp32: CFLAGS += -DHAVE_GP32 -funroll-loops -finline-functions -mcpu=arm9tdmi \
		-mstructure-size-boundary=32
gp32: LDFLAGS += -lgpmem -lgpos -lgpstdio -lgpstdlib -lgpgraphic
gp32: ROMPATH = /gpmm/dragon
gp32: xroar.fxe

solaris: CFLAGS += $(CFLAGS_SDL) $(CFLAGS_GTK)
solaris: LDFLAGS += $(LDFLAGS_SDL) $(LDFLAGS_GTK)
solaris: xroar

solaris-le: CFLAGS += -DWRONG_ENDIAN
solaris-le: solaris

macosx: CFLAGS += $(CFLAGS_SDL) $(CFLAGS_GL)
macosx: OPT = -fast -mcpu=7450 -mdynamic-no-pic
macosx: LDFLAGS_GL = -framework OpenGL
macosx: LDFLAGS += $(LDFLAGS_SDL) $(LDFLAGS_GL)
macosx: ROMPATH = :~/Library/XRoar/Roms:/Library/XRoar/Roms
macosx: xroar

macosx-le: CFLAGS += -DWRONG_ENDIAN
macosx-le: macosx

windows32: TOOL_PREFIX := i586-mingw32-
windows32: CFLAGS += $(CFLAGS_SDL) $(CFLAGS_GL) $(CFLAGS_WRONGENDIAN) -DWINDOWS32 -DPREFER_NOYUV
windows32: LDFLAGS += $(LDFLAGS_SDL) $(LDFLAGS_GL)
windows32: SDL_CONFIG := /usr/local/i586-mingw32/bin/sdl-config
windows32: LDFLAGS_GL = -lopengl32
windows32: ROMPATH = .
windows32: xroar.exe

HOSTCC = gcc
CC = $(TOOL_PREFIX)gcc
AS = $(TOOL_PREFIX)as
OBJCOPY = $(TOOL_PREFIX)objcopy

COMMON_OBJS = xroar.o snapshot.o tape.o hexs19.o machine.o m6809.o sam.o \
		 pia.o wd2797.o vdg.o video.o sound.o ui.o keyboard.o \
		 joystick.o events.o

UNIX_OBJS = fs_unix.o joystick_sdl.o keyboard_sdl.o main_unix.o \
	       sound_jack.o sound_null.o sound_oss.o sound_sdl.o sound_sun.o \
	       sound_macosx.o ui_carbon.o ui_cli.o ui_gtk.o ui_windows32.o \
	       video_sdl.o video_sdlyuv.o video_sdlgl.o

GP32_OBJS = gp32/crt0.o fs_gp32.o keyboard_gp32.o main_gp32.o sound_gp32.o \
	       ui_gp32.o video_gp32.o gp32/gpstart.o gp32/udaiis.o \
	       gp32/gpsound.o gp32/gpkeypad.o gp32/gpchatboard.o \
	       cmode_bin.o copyright.o kbd_graphics.o

ALL_OBJS = $(COMMON_OBJS) $(UNIX_OBJS) $(GP32_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

xroar.fxe: $(GP32_OBJS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -nostartfiles -o xroar.elf -T gp32/lnkscript $^ $(LDFLAGS)
	$(OBJCOPY) -O binary xroar.elf xroar.bin
	b2fxec -t "XRoar" -a "Ciaran Anscomb" -b gp32/icon.bmp -f xroar.bin $@

xroar xroar.exe: $(UNIX_OBJS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

./prerender: prerender.c
	$(HOSTCC) -o $@ $<

video_gp32.c: vdg_bitmaps_gp32.c

vdg_bitmaps_gp32.c: ./prerender vdg_bitmaps.c
	./prerender > $@

./img2c: img2c.c
	$(HOSTCC) $(CFLAGS_SDL) -o $@ $< $(LDFLAGS_SDL) $(LDFLAGS_SDLIMAGE)

copyright.c: ./img2c gp32/copyright.png
	./img2c copyright_bin gp32/copyright.png > $@

cmode_bin.c: ./img2c gp32/cmode_kbd.png gp32/cmode_cur.png gp32/cmode_joyr.png gp32/cmode_joyl.png
	./img2c cmode_bin gp32/cmode_kbd.png gp32/cmode_cur.png gp32/cmode_joyr.png gp32/cmode_joyl.png > $@

kbd_graphics.c: ./img2c gp32/kbd.png gp32/kbd_shift.png
	./img2c kbd_bin gp32/kbd.png gp32/kbd_shift.png > $@

.PHONY: clean dist dist-gp32 dist-windows32 dist-macos dist-macosx

CLEAN_FILES = xroar xroar.exe xroar.fxe $(ALL_OBJS) ./img2c ./prerender \
	copyright.c cmode_bin.c kbd_graphics.c xroar.bin xroar.elf

clean:
	rm -f $(CLEAN_FILES)

DISTNAME = xroar-$(VERSION)

dist:
	darcs dist --dist-name $(DISTNAME)
	mv $(DISTNAME).tar.gz ..

dist-gp32: gp32
	mkdir $(DISTNAME)-gp32
	cp COPYING ChangeLog README TODO xroar.fxe $(DISTNAME)-gp32/
	rm -f ../$(DISTNAME)-gp32.zip
	zip -r ../$(DISTNAME)-gp32.zip $(DISTNAME)-gp32
	rm -rf $(DISTNAME)-gp32/

dist-windows32: windows32
	mkdir $(DISTNAME)-windows32
	cp COPYING ChangeLog README TODO xroar.exe /usr/local/i586-mingw32/bin/SDL.dll $(DISTNAME)-windows32/
	i586-mingw32-strip $(DISTNAME)-windows32/xroar.exe
	i586-mingw32-strip $(DISTNAME)-windows32/SDL.dll
	rm -f ../$(DISTNAME)-windows32.zip
	zip -r ../$(DISTNAME)-windows32.zip $(DISTNAME)-windows32
	rm -rf $(DISTNAME)-windows32/

dist-macosx dist-macos: macosx
	mkdir XRoar-$(VERSION)
	mkdir -p XRoar-$(VERSION)/XRoar.app/Contents/MacOS XRoar-$(VERSION)/XRoar.app/Contents/Frameworks XRoar-$(VERSION)/XRoar.app/Contents/Resources
	cp xroar XRoar-$(VERSION)/XRoar.app/Contents/MacOS/
	cp /usr/local/lib/libSDL-1.2.0.dylib XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/
	install_name_tool -change /usr/local/lib/libSDL-1.2.0.dylib @executable_path/../Frameworks/libSDL-1.2.0.dylib XRoar-$(VERSION)/XRoar.app/Contents/MacOS/xroar
	strip XRoar-$(VERSION)/XRoar.app/Contents/MacOS/xroar
	strip -x XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/libSDL-1.2.0.dylib
	sed -e "s!@VERSION@!$(VERSION)!g" macos/Info.plist.in > XRoar-$(VERSION)/XRoar.app/Contents/Info.plist
	cp macos/xroar.icns XRoar-$(VERSION)/XRoar.app/Contents/Resources/
	cp README COPYING ChangeLog TODO XRoar-$(VERSION)/
	chmod -R o+rX,g+rX XRoar-$(VERSION)/
	hdiutil create -srcfolder XRoar-$(VERSION) -uid 99 -gid 99 ../XRoar-$(VERSION).dmg
	rm -rf XRoar-$(VERSION)/
