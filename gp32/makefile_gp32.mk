# Makefile for GP32 build of XRoar.
# Included from main Makefile.

############################################################################
# Objects for GP32 build:

xroar_gp32_OBJS = gp32/fs_gp32.o gp32/keyboard_gp32.o gp32/main_gp32.o \
	gp32/sound_gp32.o gp32/ui_gp32.o gp32/video_gp32.o gp32/gpchatboard.o \
	gp32/gpgfx.o gp32/gpkeypad.o gp32/gplib.o gp32/gpsound.o \
	gp32/gpstart.o gp32/udaiis.o
xroar_gp32_INT_OBJS = gp32/cmode_bin.o gp32/copyright.o gp32/kbd_graphics.o \
	gp32/vdg_bitmaps_gp32.o
CLEAN += $(xroar_gp32_OBJS) $(xroar_gp32_INT_OBJS)

############################################################################
# GP32 specific build rules

ifeq ($(BUILD_STYLE),gp32)

ROMPATH = \"gp\\\\:/gpmm/dragon\"
CONFPATH = \"gp\\\\:/gpmm/dragon\"

xroar_gp32_CFLAGS = $(CFLAGS) \
	-march=armv4t -mtune=arm920t -fomit-frame-pointer -ffast-math
xroar_gp32_CPPFLAGS = $(CPPFLAGS) -I$(CURDIR) -I$(SRCROOT) $(WARN) \
	-DVERSION=\"$(VERSION)\" \
	-DROMPATH=$(ROMPATH) -DCONFPATH=$(CONFPATH)
xroar_gp32_LDFLAGS = $(LDFLAGS) -specs=gp32_gpsdk.specs \
	-lgpmem -lgpstdlib -lgpos -lgpstdio -lgpgraphic

$(xroar_gp32_OBJS): | gp32
gp32:
	mkdir -p gp32

xroar_gp32_ALL_OBJS = $(xroar_common_OBJS) $(xroar_common_INT_OBJS) \
	$(xroar_gp32_OBJS) $(xroar_gp32_INT_OBJS)

$(xroar_gp32_ALL_OBJS): $(CONFIG_FILES)

$(xroar_common_OBJS) $(xroar_gp32_OBJS): %.o: $(SRCROOT)/%.c
	$(CC) -o $@ $(xroar_gp32_CFLAGS) $(xroar_gp32_CPPFLAGS) -c $<

$(xroar_common_INT_OBJS) $(xroar_gp32_INT_OBJS): %.o: ./%.c
	$(CC) -o $@ $(xroar_gp32_CFLAGS) $(xroar_gp32_CPPFLAGS) -c $<

xroar.elf: $(xroar_gp32_ALL_OBJS)
	$(CC) -o $@ $(xroar_gp32_ALL_OBJS) $(xroar_gp32_LDFLAGS)

xroar.fxe: xroar.elf
	$(OBJCOPY) -O binary xroar.elf xroar.bin
	b2fxec -t "XRoar $(VERSION)" -a "Ciaran Anscomb" -b $(SRCROOT)/gp32/icon.bmp -f xroar.bin $@

.PHONY: build-bin
build-bin: xroar.fxe

endif

CLEAN += xroar.elf xroar.fxe

############################################################################
# Generated dependencies and the tools that generate them

.SECONDARY: tools/prerender
tools/prerender: $(SRCROOT)/tools/prerender.c $(SRCROOT)/vdg_bitmaps.c | tools
	$(BUILD_CC) -o $@ $<

.SECONDARY: tools/img2c
tools/img2c: $(SRCROOT)/tools/img2c.c | tools
	$(BUILD_CC) $(BUILD_SDL_CFLAGS) -o $@ $< $(BUILD_SDL_LDFLAGS) $(BUILD_SDL_IMAGE_LDFLAGS)

CLEAN += tools/img2c tools/prerender

#

$(SRCROOT)/video_gp32.c: gp32/vdg_bitmaps_gp32.c

gp32/vdg_bitmaps_gp32.c: tools/prerender
	tools/prerender > $@

gp32/copyright.c: tools/img2c $(SRCROOT)/gp32/copyright.png
	tools/img2c copyright_bin $(SRCROOT)/gp32/copyright.png > $@

gp32/cmode_bin.c: tools/img2c $(SRCROOT)/gp32/cmode_kbd.png $(SRCROOT)/gp32/cmode_cur.png $(SRCROOT)/gp32/cmode_joyr.png $(SRCROOT)/gp32/cmode_joyl.png
	tools/img2c cmode_bin $(SRCROOT)/gp32/cmode_kbd.png $(SRCROOT)/gp32/cmode_cur.png $(SRCROOT)/gp32/cmode_joyr.png $(SRCROOT)/gp32/cmode_joyl.png > $@

gp32/kbd_graphics.c: tools/img2c $(SRCROOT)/gp32/kbd.png $(SRCROOT)/gp32/kbd_shift.png
	tools/img2c kbd_bin $(SRCROOT)/gp32/kbd.png $(SRCROOT)/gp32/kbd_shift.png > $@

CLEAN += gp32/vdg_bitmaps_gp32.c gp32/copyright.c gp32/cmode_bin.c \
	gp32/kbd_graphics.c

############################################################################
# Distribution creation

.PHONY: dist-gp32
dist-gp32: build-bin doc/xroar.pdf
	mkdir $(DISTNAME)-gp32
	cp COPYING.GPL ChangeLog README doc/xroar.pdf xroar.fxe $(DISTNAME)-gp32/
	rm -f ../$(DISTNAME)-gp32.zip
	zip -r ../$(DISTNAME)-gp32.zip $(DISTNAME)-gp32
	rm -rf $(DISTNAME)-gp32/
