### These settings set what compiler to use.  Note that when building for
### GP32, certain intermediate tools are built using CC_UNIX, so that still
### needs to be set correctly.

CC_UNIX = gcc

# If you want to build a GP32 binary ('make xroar.fxe'), make sure these
# are set to point to the appropriate arm-elf build tools (and that b2fxec
# is in your path).
CC_GP32 = arm-elf-gcc
AS_GP32= arm-elf-as
OBJCOPY_GP32 = arm-elf-objcopy

# If you want to build a Windows32 binary ('make xroar.exe'), make sure
# CC_WINDOWS32 is set to the right cross-compiler, and WINDOWS32_PREFIX
# points to the top of your MinGW32 install (currently only used to find
# sdl-config).
CC_WINDOWS32 = i586-mingw32-gcc
WINDOWS32_PREFIX = /usr/local/i586-mingw32

CFLAGS_GP32 = -O3 -funroll-loops -finline-functions -mcpu=arm9tdmi \
	-mstructure-size-boundary=32 -finline-limit=320000

CFLAGS_UNIX = -O3 -g -finline-limit=63000

CFLAGS_COMMON = -Wall -W -Wstrict-prototypes -Wpointer-arith -Wcast-align \
	-Wcast-qual -Wshadow -Waggregate-return -Wnested-externs -Winline \
	-Wwrite-strings -Wundef -Wsign-compare -Wmissing-prototypes \
	-Wredundant-decls

### Specify the features you want by uncommenting (or re-commenting) lines
### from the following.  Note that these do not affect the specialised GP32
### build.

# Video, audio and user-interface modules:
USE_SDL = 1		# SDL video and audio modules
#USE_OSS_AUDIO = 1	# OSS blocking audio
#USE_JACK_AUDIO = 1	# Connects to JACK audio server
#USE_SUN_AUDIO = 1	# Sun audio.  Might suit *BSD too, don't know
#USE_NULL_AUDIO = 1	# Requires Linux RTC as sound is used to sync
USE_GTK_UI = 1		# Simple GTK+ file-requester
USE_CLI_UI = 1		# Prompt for filenames on command line
#USE_CARBON_UI = 1	# MacOS X Carbon UI

# Build for a little-endian machine, eg x86.  This should be commented out
# for big-endian architectures, eg Sparc.
CFLAGS_UNIX += -DWRONG_ENDIAN

# Uncomment this to enable tracing
#CFLAGS_UNIX += -DTRACE

# Uncomment this to default to building a GP32 binary (you can
# 'make xroar.fxe' manually anyway).
#BUILD_GP32 = 1

###
### ----- You shouldn't need to change anything under this line ------ ###
###

COMMON_SOURCES_H = config.h fs.h hexs19.h joystick.h keyboard.h logging.h \
	m6809.h machine.h pia.h sam.h snapshot.h sound.h tape.h types.h ui.h \
	vdg.h video.h wd2797.h xroar.h
COMMON_SOURCES_C = xroar.c snapshot.c tape.c hexs19.c machine.c m6809.c sam.c \
	pia.c wd2797.c vdg.c video.c sound.c ui.c keyboard.c joystick.c
COMMON_SOURCES = $(COMMON_SOURCES_H) $(COMMON_SOURCES_C)
COMMON_OBJECTS = $(COMMON_SOURCES_C:.c=.o)

UNIX_TARGET = xroar
UNIX_SOURCES_H = fs_unix.h
UNIX_SOURCES_C = fs_unix.c joystick_sdl.c keyboard_sdl.c main_unix.c \
	sound_jack.c sound_null.c sound_oss.c sound_sdl.c sound_sun.c \
	ui_carbon.c ui_cli.c ui_gtk.c ui_windows32.c video_sdl.c video_sdlyuv.c
UNIX_SOURCES = $(UNIX_SOURCES_H) $(UNIX_SOURCES_C)
UNIX_OBJECTS = $(UNIX_SOURCES_C:.c=.o)

WINDOWS32_TARGET = xroar.exe

GP32_TARGET = xroar.fxe
GP32_SOURCES_H = fs_gp32.h video_gp32.h
GP32_SOURCES_C = fs_gp32.c keyboard_gp32.c main_gp32.c sound_gp32.c ui_gp32.c \
	video_gp32.c gp32/gpstart.c gp32/udaiis.c gp32/gpsound.c \
	gp32/gpkeypad.c gp32/gpchatboard.c
GP32_SOURCES_S = gp32/crt0.s
GP32_SOURCES = $(GP32_SOURCES_H) $(GP32_SOURCES_C)
GP32_BUILD_SOURCES_C = cmode_bin.c copyright.c kbd_graphics.c
GP32_OBJECTS = $(GP32_SOURCES_S:.s=.o) $(GP32_SOURCES_C:.c=.o) \
	$(GP32_BUILD_SOURCES_C:.c=.o)

EXTRA_SOURCES = img2c.c prerender.c vdg_bitmaps.c keyboard_sdl_mappings.c
EXTRA_CLEAN = xroar.bin xroar.elf img2c prerender vdg_bitmaps_gp32.c

CLEAN = $(COMMON_OBJECTS) $(UNIX_OBJECTS) $(GP32_OBJECTS) \
	$(UNIX_TARGET) $(GP32_TARGET) $(WINDOWS32_TARGET) \
	$(GP32_BUILD_SOURCES_C) $(EXTRA_CLEAN)

SDL_CONFIG = sdl-config
xroar.exe: SDL_CONFIG = $(WINDOWS32_PREFIX)/bin/sdl-config

ifdef USE_SDL
	CFLAGS_UNIX += -DHAVE_SDL_VIDEO -DHAVE_SDL_AUDIO
	CFLAGS_SDL = $(shell $(SDL_CONFIG) --cflags)
	LDFLAGS_SDL = $(shell $(SDL_CONFIG) --libs)
endif

ifdef USE_OSS_AUDIO
	CFLAGS_UNIX += -DHAVE_OSS_AUDIO
endif
ifdef USE_JACK_AUDIO
	CFLAGS_UNIX += -DHAVE_JACK_AUDIO
	LDFLAGS_JACK = -ljack -lpthread
endif
ifdef USE_SUN_AUDIO
	CFLAGS_UNIX += -DHAVE_SUN_AUDIO
endif
ifdef USE_NULL_AUDIO
	CFLAGS_UNIX += -DHAVE_NULL_AUDIO
endif

ifdef USE_GTK_UI
	CFLAGS_UNIX += -DHAVE_GTK_UI
	CFLAGS_GTK = $(shell gtk-config --cflags)
	LDFLAGS_GTK = $(shell gtk-config --libs)
endif
ifdef USE_CLI_UI
	CFLAGS_UNIX += -DHAVE_CLI_UI
endif
ifdef USE_CARBON_UI
	CFLAGS_UNIX += -DHAVE_CARBON_UI
	LDFLAGS_CARBON = -framework Carbon
endif

CFLAGS_UNIX += $(CFLAGS_SDL) $(CFLAGS_GTK)
LDFLAGS_UNIX = $(LDFLAGS_SDL) $(LDFLAGS_JACK) $(LDFLAGS_GTK) $(LDFLAGS_CARBON)
CFLAGS_GP32 += -DHAVE_GP32
LDFLAGS_GP32 = -lgpmem -lgpos -lgpstdio -lgpstdlib -lgpgraphic

all: xroar

xroar: CC = $(CC_UNIX)
xroar: CFLAGS = $(CFLAGS_UNIX) $(CFLAGS_COMMON)
xroar.exe: CC = $(CC_WINDOWS32)
xroar.exe: CFLAGS = -DWINDOWS32 $(CFLAGS_UNIX) $(CFLAGS_COMMON)
xroar xroar.exe: $(UNIX_OBJECTS) $(COMMON_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(UNIX_OBJECTS) $(COMMON_OBJECTS) $(LDFLAGS_UNIX)

xroar.fxe: CC = $(CC_GP32)
xroar.fxe: AS = $(AS_GP32)
xroar.fxe: OBJCOPY = $(OBJCOPY_GP32)
xroar.fxe: CFLAGS = $(CFLAGS_GP32) $(CFLAGS_COMMON)
xroar.fxe: $(GP32_OBJECTS) $(COMMON_OBJECTS)
	$(CC_GP32) $(CFLAGS) -nostartfiles -o xroar.elf -T gp32/lnkscript $(GP32_OBJECTS) $(COMMON_OBJECTS) $(LDFLAGS_GP32)
	$(OBJCOPY) -O binary xroar.elf xroar.bin
	b2fxec -t "XRoar" -a "Ciaran Anscomb" -b gp32/icon.bmp -f xroar.bin $@

IMG2C = $(CURDIR)/img2c
PRERENDER = $(CURDIR)/prerender

$(IMG2C): img2c.c
	$(CC_UNIX) $(shell $(SDL_CONFIG) --cflags) -o $@ img2c.c $(shell $(SDL_CONFIG) --libs) -lSDL_image

$(PRERENDER): prerender.c
	$(CC_UNIX) -o $@ prerender.c

vdg_bitmaps_gp32.c: $(PRERENDER) vdg_bitmaps.c
	$(PRERENDER) > $@

video_gp32.c: vdg_bitmaps_gp32.c
	#touch $@

copyright.c: $(IMG2C) gp32/copyright.png
	$(IMG2C) copyright_bin gp32/copyright.png > $@

cmode_bin.c: $(IMG2C) gp32/cmode_kbd.png gp32/cmode_cur.png gp32/cmode_joyr.png gp32/cmode_joyl.png
	$(IMG2C) cmode_bin gp32/cmode_kbd.png gp32/cmode_cur.png gp32/cmode_joyr.png gp32/cmode_joyl.png > $@

kbd_graphics.c: $(IMG2C) gp32/kbd.png gp32/kbd_shift.png
	$(IMG2C) kbd_bin gp32/kbd.png gp32/kbd_shift.png > $@

clean:
	rm -f $(CLEAN)
