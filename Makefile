CC = gcc

#CFLAGS  = -O3 -Wall
CFLAGS  = -g -Wall

#CFLAGS += -DAUTOSNAP_HACK=\"snaps/frogger.sna\"

# Uncomment this to enable tracing
#CFLAGS += -DTRACE

# Choose what you're building for and uncomment the relevant lines:

# Uncomment this if you're building a GP32 binary.  This overrides all the
# other options, as it's a very specific target.
BUILD_GP32 = 1

# Build for a little-endian machine, eg x86.  This should be commented out
# for big-endian architectures, eg Sparc.
CFLAGS += -DWRONG_ENDIAN

# Which video drivers to compile in?
BUILD_SDL_VIDEO = 1	# 'sdl' (fixed) and 'sdlyuv' (should be accelerated)
#BUILD_X11_VIDEO = 1	# Should write a raw X11 driver.  Haven't yet.

# Which audio drivers?
BUILD_SDL_AUDIO = 1	# Use the SDL's callback-based audio
BUILD_OSS_AUDIO = 1	# OSS blocking audio
BUILD_JACK_AUDIO = 1	# Connects to JACK audio server
#BUILD_SUN_AUDIO = 1	# Sun audio.  Might suit *BSD too, don't know
BUILD_NULL_AUDIO = 1	# Requires Linux RTC as sound is used to sync

# And which user-interfaces?
BUILD_GTK_UI = 1	# Simple GTK+ file-requester
BUILD_CLI_UI = 1	# Prompt for filenames on command line
#BUILD_CARBON_UI = 1	# MacOS X Carbon UI

# Comment this out if your system doesn't have int_fastN_t (eg, Solaris)
CFLAGS += -DHAVE_FASTINT

# ----- You shouldn't need to change anything under this line ------ #

ifdef BUILD_GP32
	HOST_CC = gcc
	CC = arm-elf-gcc
	AS = arm-elf-as
	OBJCOPY = arm-elf-objcopy

	CFLAGS = -O3 -DHAVE_GP32 -Wall -funroll-loops -finline-functions -mcpu=arm9tdmi -mstructure-size-boundary=32 -finline-limit=320000
	LDFLAGS = -lgpmem -lgpos -lgpstdio -lgpstdlib -lgpgraphic
	TARGET_BIN = xroar.fxe
	TARGET_SRC = main_gp32.c fs_gp32.c copyright.c cmode_bin.c kbd_graphics.c video_gp32.c sound_gp32.c ui_gp32.c keyboard_gp32.c
	TARGET_OBJ = gp32/crt0.o gp32/gpstart.o gp32/udaiis.o gp32/gpsound.o gp32/gpkeypad.o gp32/gpchatboard.o
	CLEAN_SUPP = xroar.bin xroar.elf xroar.map vdg_bitmaps_gp32.c img2c prerender copyright.c cmode_bin.c kbd_graphics.
else
	ifdef BUILD_SDL_VIDEO
		CFLAGS += -DHAVE_SDL_VIDEO
		CFLAGS_SDL = $(shell sdl-config --cflags)
		LDFLAGS_SDL = $(shell sdl-config --libs)
	endif

	ifdef BUILD_SDL_AUDIO
		CFLAGS += -DHAVE_SDL_AUDIO
		CFLAGS_SDL = $(shell sdl-config --cflags)
		LDFLAGS_SDL = $(shell sdl-config --libs)
	endif
	ifdef BUILD_OSS_AUDIO
		CFLAGS += -DHAVE_OSS_AUDIO
	endif
	ifdef BUILD_JACK_AUDIO
		CFLAGS += -DHAVE_JACK_AUDIO
		LDFLAGS_JACK = -ljack -lpthread
	endif
	ifdef BUILD_SUN_AUDIO
		CFLAGS += -DHAVE_SUN_AUDIO
	endif
	ifdef BUILD_NULL_AUDIO
		CFLAGS += -DHAVE_NULL_AUDIO
	endif

	ifdef BUILD_GTK_UI
		CFLAGS += -DHAVE_GTK_UI
		CFLAGS_GTK = $(shell gtk-config --cflags)
		LDFLAGS_GTK = $(shell gtk-config --libs)
	endif
	ifdef BUILD_CLI_UI
		CFLAGS += -DHAVE_CLI_UI
	endif
	ifdef BUILD_CARBON_UI
		CFLAGS += -DHAVE_CARBON_UI
		LDFLAGS_CARBON = -framework Carbon
	endif

	CFLAGS += $(CFLAGS_SDL) $(CFLAGS_GTK)
	LDFLAGS = $(LDFLAGS_SDL) $(LDFLAGS_JACK) $(LDFLAGS_GTK) $(LDFLAGS_CARBON)
	TARGET_BIN = xroar
	TARGET_SRC = main_unix.c fs_unix.c \
		video_sdl.c video_sdlyuv.c \
		sound_sdl.c sound_oss.c sound_jack.c sound_sun.c sound_null.c \
		ui_gtk.c ui_cli.c ui_carbon.c \
		keyboard_sdl.c joystick_sdl.c
endif

SRCS = $(TARGET_SRC) xroar.c \
	snapshot.c tape.c hexs19.c \
	machine.c m6809.c sam.c pia.c wd2797.c vdg.c \
	video.c sound.c ui.c keyboard.c joystick.c

OBJS = $(TARGET_OBJ) $(SRCS:.c=.o)

all: $(TARGET_BIN)

xroar: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

xroar.elf: $(OBJS)
	$(CC) $(CFLAGS) -nostartfiles -Wl,-Map,xroar.map -o $@ -T gp32/lnkscript $(OBJS) $(LDFLAGS)

xroar.bin: xroar.elf
	$(OBJCOPY) -O binary $< $@

xroar.fxe: xroar.bin
	b2fxec -t "XRoar" -a "Ciaran Anscomb" -b gp32/icon.bmp -f $< $@

IMG2C = $(CURDIR)/img2c
PRERENDER = $(CURDIR)/prerender

$(IMG2C): img2c.c
	$(HOST_CC) $(shell sdl-config --cflags) -o $@ img2c.c $(shell sdl-config --libs) -lSDL_image

$(PRERENDER): prerender.c
	$(HOST_CC) -o $@ prerender.c

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
	rm -f $(TARGET_BIN) $(CLEAN_SUPP) $(OBJS)
