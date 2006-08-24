### Install under this prefix:
prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
libdir = $(exec_prefix)/lib
mandir = $(prefix)/man
datadir = $(prefix)/share

VERSION := 0.18

.PHONY: all usage gp32 linux macosx solaris windows32

all:
	@sndfile=""; pkg-config sndfile && sndfile="SNDFILE=1"; \
	 case `uname -s` in \
	 Linux) $(MAKE) $$sndfile linux; ;; \
	 Darwin) $(MAKE) $$sndfile macosx; ;; \
	 SunOS) $(MAKE) $$sndfile solaris; ;; \
	 *) $(MAKE) usage; ;; \
	 esac; \
	 exit 0

usage:
	@echo
	@echo "Usage: \"make system-type\", where system-type is one of:"
	@echo
	@echo " gp32 linux macosx solaris windows32"
	@echo
	@echo "You can append options to determine which tools are used to compile."
	@echo "e.g., \"make gp32 TARGET_ARCH=arm-elf\""
	@echo

OPT = -O3 -pipe
macosx: OPT = -fast -mcpu=7450 -mdynamic-no-pic -pipe
WARN = -Wall -W -Wstrict-prototypes -Wpointer-arith -Wcast-align \
	-Wcast-qual -Wshadow -Waggregate-return -Wnested-externs -Winline \
	-Wwrite-strings -Wundef -Wsign-compare -Wmissing-prototypes \
	-Wredundant-decls

CFLAGS = $(OPT) $(WARN) -g -DVERSION=\"$(VERSION)\" -DROMPATH=\"$(ROMPATH)\"

ENABLE_SDL       := linux solaris macosx windows32
ENABLE_SDLGL     := linux macosx windows32
ENABLE_OSS       := linux
ENABLE_SUN_AUDIO := solaris
ENABLE_COREAUDIO := macosx
ENABLE_RTC       := linux
ENABLE_JACK      :=
ENABLE_GTK       := linux solaris
ENABLE_CARBON    := macosx
ENABLE_WINDOWS32 := windows32
ENABLE_SNDFILE   := windows32

USES_UNIX_TARGET      := linux solaris macosx
USES_GP32_TARGET      := gp32
USES_WINDOWS32_TARGET := windows32

gp32 dist-gp32: TARGET_ARCH=arm-elf
windows32 dist-windows32: TARGET_ARCH=i586-mingw32

ifdef SDLGL
	ENABLE_SDLGL += $(MAKECMDGOALS)
endif
ifdef OSS
	ENABLE_OSS += $(MAKECMDGOALS)
endif
ifdef SUN_AUDIO
	ENABLE_SUN_AUDIO += $(MAKECMDGOALS)
endif
ifdef COREAUDIO
	ENABLE_COREAUDIO += $(MAKECMDGOALS)
endif
ifdef RTC
	ENABLE_RTC += $(MAKECMDGOALS)
endif
ifdef JACK
	ENABLE_JACK += $(MAKECMDGOALS)
endif
ifdef CARBON
	ENABLE_CARBON += $(MAKECMDGOALS)
endif
ifdef CLI
	ENABLE_CLI += $(MAKECMDGOALS)
endif
ifdef TRACE
	ENABLE_TRACE += $(MAKECMDGOALS)
endif
ifdef SNDFILE
	ENABLE_SNDFILE += $(MAKECMDGOALS)
endif

COMMON_OBJS := xroar.o snapshot.o tape.o hexs19.o machine.o m6809.o \
		sam.o pia.o wd2797.o vdg.o module.o \
		keyboard.o joystick.o events.o vdrive.o vdisk.o cart.o \
		crc16.o sound_null.o
ALL_OBJS := $(COMMON_OBJS)

OBJS_SDL := ui_sdl.o video_sdl.o video_sdlyuv.o sound_sdl.o keyboard_sdl.o joystick_sdl.o
ALL_OBJS += $(OBJS_SDL)
SDL_CONFIG = $(TOOL_PREFIX)sdl-config
CFLAGS_SDL = -DHAVE_SDL $(shell $(SDL_CONFIG) --cflags)
LDFLAGS_SDL = $(shell $(SDL_CONFIG) --libs)
HOST_SDL_CONFIG = sdl-config
HOST_CFLAGS_SDL = $(shell $(HOST_SDL_CONFIG) --cflags)
HOST_LDFLAGS_SDL = $(shell $(HOST_SDL_CONFIG) --libs)

OBJS_SDLGL := video_sdlgl.o
ALL_OBJS += $(OBJS_SDLGL)
CFLAGS_SDLGL = -DHAVE_SDLGL
LDFLAGS_SDLGL = -lGL
macosx: LDFLAGS_SDLGL = -framework OpenGL
windows32: LDFLAGS_SDLGL = -lopengl32

OBJS_OSS := sound_oss.o
ALL_OBJS += $(OBJS_OSS)
CFLAGS_OSS = -DHAVE_OSS_AUDIO

OBJS_SUN_AUDIO := sound_sun.o
ALL_OBJS += $(OBJS_SUN_AUDIO)
CFLAGS_SUN_AUDIO = -DHAVE_SUN_AUDIO -DPREFER_NOYUV

OBJS_COREAUDIO := sound_macosx.o
ALL_OBJS += $(OBJS_COREAUDIO)
CFLAGS_COREAUDIO = -DHAVE_MACOSX_AUDIO
LDFLAGS_COREAUDIO = -framework CoreAudio

OBJS_RTC := sound_rtc.o
ALL_OBJS += $(OBJS_RTC)
CFLAGS_RTC = -DHAVE_RTC

OBJS_JACK = sound_jack.o
ALL_OBJS += $(OBJS_JACK)
CFLAGS_JACK = -DHAVE_JACK_AUDIO
LDFLAGS_JACK = -ljack -lpthread

OBJS_GTK := filereq_gtk.o
ALL_OBJS += $(OBJS_GTK)
GTK_CONFIG = gtk-config
CFLAGS_GTK = -DHAVE_GTK $(shell $(GTK_CONFIG) --cflags)
LDFLAGS_GTK = $(shell $(GTK_CONFIG) --libs)

OBJS_CARBON := filereq_carbon.o
ALL_OBJS += $(OBJS_CARBON)
CFLAGS_CARBON = -DHAVE_CARBON
LDFLAGS_CARBON = -framework Carbon

OBJS_WINDOWS32 := common_windows32.o filereq_windows32.o
ALL_OBJS += $(OBJS_WINDOWS32)
CFLAGS_WINDOWS32 = -DWINDOWS32 -DPREFER_NOYUV
LDFLAGS_WINDOWS32 =

OBJS_CLI = filereq_cli.o
ALL_OBJS += $(OBJS_CLI)
CFLAGS_CLI = -DHAVE_CLI

OBJS_TRACE = m6809_dasm.o
ALL_OBJS += $(OBJS_TRACE)
CFLAGS_TRACE = -DTRACE

CFLAGS_SNDFILE = -DHAVE_SNDFILE $(shell $(PKG_CONFIG) --cflags sndfile)
LDFLAGS_SNDFILE = $(shell $(PKG_CONFIG) --libs sndfile)

ROMPATH = :~/.xroar/roms:$(prefix)/share/xroar/roms
windows32: ROMPATH = .
macosx: ROMPATH = :~/.xroar/roms:~/Library/XRoar/Roms
gp32: ROMPATH = gp:/gpmm/dragon

# Enable SDL for these targets:
$(ENABLE_SDL): OBJS    += $(OBJS_SDL)
$(ENABLE_SDL): CFLAGS  += $(CFLAGS_SDL)
$(ENABLE_SDL): LDFLAGS += $(LDFLAGS_SDL)
$(ENABLE_SDL): $(OBJS_SDL)

# Enable SDLGL for these targets:
$(ENABLE_SDLGL): OBJS    += $(OBJS_SDLGL)
$(ENABLE_SDLGL): CFLAGS  += $(CFLAGS_SDLGL)
$(ENABLE_SDLGL): LDFLAGS += $(LDFLAGS_SDLGL)
$(ENABLE_SDLGL): $(OBJS_SDLGL)

# Enable OSS for these targets:
$(ENABLE_OSS): OBJS    += $(OBJS_OSS)
$(ENABLE_OSS): CFLAGS  += $(CFLAGS_OSS)
$(ENABLE_OSS): LDFLAGS += $(LDFLAGS_OSS)
$(ENABLE_OSS): $(OBJS_OSS)

# Enable Sun audio for these targets:
$(ENABLE_SUN_AUDIO): OBJS    += $(OBJS_SUN_AUDIO)
$(ENABLE_SUN_AUDIO): CFLAGS  += $(CFLAGS_SUN_AUDIO)
$(ENABLE_SUN_AUDIO): LDFLAGS += $(LDFLAGS_SUN_AUDIO)
$(ENABLE_SUN_AUDIO): $(OBJS_SUN_AUDIO)
$(ENABLE_SUN_AUDIO): $(OBJS_SUN_AUDIO)

# Enable CoreAudio for these targets:
$(ENABLE_COREAUDIO): OBJS    += $(OBJS_COREAUDIO)
$(ENABLE_COREAUDIO): CFLAGS  += $(CFLAGS_COREAUDIO)
$(ENABLE_COREAUDIO): LDFLAGS += $(LDFLAGS_COREAUDIO)
$(ENABLE_COREAUDIO): $(OBJS_COREAUDIO)

# Enable RTC for these targets:
$(ENABLE_RTC): OBJS    += $(OBJS_RTC)
$(ENABLE_RTC): CFLAGS  += $(CFLAGS_RTC)
$(ENABLE_RTC): LDFLAGS += $(LDFLAGS_RTC)
$(ENABLE_RTC): $(OBJS_RTC)

# Enable JACK for these targets:
$(ENABLE_JACK): OBJS    += $(OBJS_JACK)
$(ENABLE_JACK): CFLAGS  += $(CFLAGS_JACK)
$(ENABLE_JACK): LDFLAGS += $(LDFLAGS_JACK)
$(ENABLE_JACK): $(OBJS_JACK)

# Enable GTK+ for these targets:
$(ENABLE_GTK): OBJS    += $(OBJS_GTK)
$(ENABLE_GTK): CFLAGS  += $(CFLAGS_GTK)
$(ENABLE_GTK): LDFLAGS += $(LDFLAGS_GTK)
$(ENABLE_GTK): $(OBJS_GTK)

# Enable Carbon for these targets:
$(ENABLE_CARBON): OBJS    += $(OBJS_CARBON)
$(ENABLE_CARBON): CFLAGS  += $(CFLAGS_CARBON)
$(ENABLE_CARBON): LDFLAGS += $(LDFLAGS_CARBON)
$(ENABLE_CARBON): $(OBJS_CARBON)

# Enable Windows32 for these targets:
$(ENABLE_WINDOWS32): OBJS    += $(OBJS_WINDOWS32)
$(ENABLE_WINDOWS32): CFLAGS  += $(CFLAGS_WINDOWS32)
$(ENABLE_WINDOWS32): LDFLAGS += $(LDFLAGS_WINDOWS32)
$(ENABLE_WINDOWS32): $(OBJS_WINDOWS32)

# Enable CLI for these targets:
$(ENABLE_CLI): OBJS    += $(OBJS_CLI)
$(ENABLE_CLI): CFLAGS  += $(CFLAGS_CLI)
$(ENABLE_CLI): LDFLAGS += $(LDFLAGS_CLI)
$(ENABLE_CLI): $(OBJS_CLI)

## Enable ZLIB for these targets:
#$(ENABLE_ZLIB): OBJS    += $(OBJS_ZLIB)
#$(ENABLE_ZLIB): CFLAGS  += $(CFLAGS_ZLIB)
#$(ENABLE_ZLIB): LDFLAGS += $(LDFLAGS_ZLIB)
#$(ENABLE_ZLIB): $(OBJS_ZLIB)

# Enable tracing (CPU debugging) for these targets:
$(ENABLE_TRACE): OBJS    += $(OBJS_TRACE)
$(ENABLE_TRACE): CFLAGS  += $(CFLAGS_TRACE)
$(ENABLE_TRACE): LDFLAGS += $(LDFLAGS_TRACE)
$(ENABLE_TRACE): $(OBJS_TRACE)

# Enable use of libsndfile for these targets:
$(ENABLE_SNDFILE): OBJS    += $(OBJS_SNDFILE)
$(ENABLE_SNDFILE): CFLAGS  += $(CFLAGS_SNDFILE)
$(ENABLE_SNDFILE): LDFLAGS += $(LDFLAGS_SNDFILE)
$(ENABLE_SNDFILE): $(OBJS_SNDFILE)

# Target-specific objects:
OBJS_UNIX := fs_unix.o main_unix.o
ALL_OBJS += $(OBJS_UNIX)

OBJS_GP32 = gp32/crt0.o fs_gp32.o main_gp32.o keyboard_gp32.o sound_gp32.o \
		ui_gp32.o video_gp32.o gp32/gpstart.o gp32/udaiis.o \
		gp32/gpgfx.o gp32/gpsound.o gp32/gpkeypad.o gp32/gpchatboard.o \
		cmode_bin.o copyright.o kbd_graphics.o
ALL_OBJS += $(OBJS_GP32)
#CFLAGS_GP32 = -DHAVE_GP32 -mcpu=arm9tdmi -funroll-loops
CFLAGS_GP32 = -DHAVE_GP32 -mcpu=arm9tdmi
LDFLAGS_GP32 = -nostartfiles -T gp32/lnkscript -lgpmem -lgpos -lgpstdio \
		-lgpstdlib -lgpgraphic

$(USES_UNIX_TARGET): OBJS += $(OBJS_UNIX)
$(USES_UNIX_TARGET): $(OBJS_UNIX) xroar

$(USES_GP32_TARGET): OBJS    += $(OBJS_GP32)
$(USES_GP32_TARGET): CFLAGS  += $(CFLAGS_GP32)
$(USES_GP32_TARGET): LDFLAGS += $(LDFLAGS_GP32)
$(USES_GP32_TARGET): $(OBJS_GP32) xroar.fxe

$(USES_WINDOWS32_TARGET): OBJS += $(OBJS_UNIX)
$(USES_WINDOWS32_TARGET): $(OBJS_UNIX) xroar.exe

TOOL_PREFIX = $(if $(TARGET_ARCH),$(TARGET_ARCH)-)
HOSTCC = gcc
CC = $(TOOL_PREFIX)gcc
AS = $(TOOL_PREFIX)as
OBJCOPY = $(TOOL_PREFIX)objcopy
PKG_CONFIG = $(TOOL_PREFIX)pkg-config

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

TARGETS = xroar xroar.exe xroar.elf

xroar.exe: $(OBJS_WINDOWS32)
xroar.elf: $(OBJS_GP32)

$(TARGETS): $(COMMON_OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(COMMON_OBJS) -o $@ $(LDFLAGS)

xroar.fxe: xroar.elf
	$(OBJCOPY) -O binary xroar.elf xroar.bin
	b2fxec -t "XRoar $(VERSION)" -a "Ciaran Anscomb" -b gp32/icon.bmp -f xroar.bin $@

tools/prerender: tools/prerender.c
	$(HOSTCC) -o $@ $<

video_gp32.c: vdg_bitmaps_gp32.c

vdg_bitmaps_gp32.c: tools/prerender vdg_bitmaps.c
	tools/prerender > $@

tools/img2c: tools/img2c.c
	$(HOSTCC) $(HOST_CFLAGS_SDL) -o $@ $< $(HOST_LDFLAGS_SDL) -lSDL_image

copyright.c: tools/img2c gp32/copyright.png
	tools/img2c copyright_bin gp32/copyright.png > $@

cmode_bin.c: tools/img2c gp32/cmode_kbd.png gp32/cmode_cur.png gp32/cmode_joyr.png gp32/cmode_joyl.png
	tools/img2c cmode_bin gp32/cmode_kbd.png gp32/cmode_cur.png gp32/cmode_joyr.png gp32/cmode_joyl.png > $@

kbd_graphics.c: tools/img2c gp32/kbd.png gp32/kbd_shift.png
	tools/img2c kbd_bin gp32/kbd.png gp32/kbd_shift.png > $@

.PHONY: clean dist dist-gp32 dist-windows32 dist-macos dist-macosx

CLEAN_FILES = $(TARGETS) $(ALL_OBJS) tools/img2c tools/prerender \
	copyright.c cmode_bin.c kbd_graphics.c vdg_bitmaps_gp32.c \
	xroar.bin xroar.fxe

clean:
	rm -f $(CLEAN_FILES)

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
