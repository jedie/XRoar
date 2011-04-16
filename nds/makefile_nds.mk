# Makefile for Nintendo DS build of XRoar.
# Included from main Makefile.

############################################################################
# Objects for Nintendo DS build:

xroar_nds7_OBJS = nds/main_nds7.o
xroar_nds9_OBJS = nds/fs_unix.o nds/main_nds9.o nds/ao_nds.o nds/ui_nds.o \
	nds/vo_nds.o \
	nds/ndsgfx.o nds/ndsui.o nds/ndsui_button.o nds/ndsui_filelist.o \
	nds/ndsui_keyboard.o nds/ndsui_scrollbar.o nds/ndsui_textbox.o
xroar_nds9_INT_OBJS = nds/kbd_graphics.o nds/nds_font8x8.o
CLEAN += $(xroar_nds7_OBJS) $(xroar_nds9_OBJS) $(xroar_nds9_INT_OBJS)

############################################################################
# Nintendo DS specific build rules

ifeq ($(BUILD_STYLE),nds)

ROMPATH = \"/dragon/roms:/dragon:\"
CONFPATH = \"/dragon:\"

# ARM7 part:

xroar_nds7_CFLAGS = $(CFLAGS) \
	-mcpu=arm7tdmi -mtune=arm7tdmi -fomit-frame-pointer -ffast-math \
	-mthumb -mthumb-interwork
xroar_nds7_CPPFLAGS = $(CPPFLAGS) -I$(CURDIR) -I$(SRCROOT) $(WARN) \
	-DVERSION=\"$(VERSION)\" \
	-DROMPATH=$(ROMPATH) -DCONFPATH=$(CONFPATH) \
	-DARM7
xroar_nds7_LDFLAGS = $(LDFLAGS) -specs=ds_arm7.specs -lnds7

$(xroar_nds7_OBJS) $(xroar_nds9_OBJS): | nds
nds:
	mkdir -p nds

xroar_nds7_ALL_OBJS = $(xroar_nds7_OBJS) $(xroar_nds7_INT_OBJS)

$(xroar_nds7_ALL_OBJS): $(CONFIG_FILES)

$(xroar_nds7_OBJS): %.o: $(SRCROOT)/%.c
	$(CC) -o $@ $(xroar_nds7_CFLAGS) $(xroar_nds7_CPPFLAGS) -c $<

$(xroar_nds7_INT_OBJS): %.o: ./%.c
	$(CC) -o $@ $(xroar_nds7_CFLAGS) $(xroar_nds7_CPPFLAGS) -c $<

xroar.arm7: $(xroar_nds7_ALL_OBJS)
	$(CC) -o $@ $(xroar_nds7_ALL_OBJS) $(xroar_nds7_LDFLAGS)

xroar.arm7.bin: xroar.arm7
	$(OBJCOPY) -O binary $< $@

# ARM9 part:

xroar_nds9_CFLAGS = $(CFLAGS) \
	-march=armv5te -mtune=arm946e-s -fomit-frame-pointer -ffast-math \
	-mthumb-interwork
xroar_nds9_CPPFLAGS = $(CPPFLAGS) -I$(CURDIR) -I$(SRCROOT) $(WARN) \
	-DVERSION=\"$(VERSION)\" \
	-DROMPATH=$(ROMPATH) -DCONFPATH=$(CONFPATH) \
	-DARM9
xroar_nds9_LDFLAGS = $(LDFLAGS) -specs=ds_arm9.specs -lfat -lnds9

xroar_nds9_ALL_OBJS = $(xroar_common_OBJS) $(xroar_common_INT_OBJS) \
	$(xroar_nds9_OBJS) $(xroar_nds9_INT_OBJS)

$(xroar_nds9_ALL_OBJS): $(CONFIG_FILES)

$(xroar_common_OBJS) $(xroar_nds9_OBJS): %.o: $(SRCROOT)/%.c
	$(CC) -o $@ $(xroar_nds9_CFLAGS) $(xroar_nds9_CPPFLAGS) -c $<

$(xroar_common_INT_OBJS) $(xroar_nds9_INT_OBJS): %.o: ./%.c
	$(CC) -o $@ $(xroar_nds9_CFLAGS) $(xroar_nds9_CPPFLAGS) -c $<

xroar.arm9: $(xroar_nds9_ALL_OBJS)
	$(CC) -o $@ $(xroar_nds9_ALL_OBJS) $(xroar_nds9_LDFLAGS)

xroar.arm9.bin: xroar.arm9
	$(OBJCOPY) -O binary $< $@

# Combined binary target:

xroar.nds: xroar.arm7.bin xroar.arm9.bin
	ndstool -c $@ -b $(SRCROOT)/nds/icon.bmp "XRoar $(VERSION)" -7 xroar.arm7.bin -9 xroar.arm9.bin
	dsbuild $@

.PHONY: build-bin
build-bin: xroar.nds

endif

CLEAN += xroar.arm7 xroar.arm7.bin xroar.arm9 xroar.arm9.bin \
	xroar.nds xroar.ds.gba

############################################################################
# Generated dependencies and the tools that generate them

.SECONDARY: tools/img2c_nds
tools/img2c_nds: $(SRCROOT)/tools/img2c_nds.c | tools
	$(BUILD_CC) $(opt_build_sdl_CFLAGS) -o $@ $< $(opt_build_sdl_LDFLAGS) $(opt_build_sdl_image_LDFLAGS)

CLEAN += tools/img2c_nds

#

nds/kbd_graphics.c: tools/img2c_nds $(SRCROOT)/nds/kbd.png $(SRCROOT)/nds/kbd_shift.png
	tools/img2c_nds kbd_bin $(SRCROOT)/nds/kbd.png $(SRCROOT)/nds/kbd_shift.png > $@

nds/nds_font8x8.c: tools/font2c $(SRCROOT)/nds/nds_font8x8.png
	tools/font2c --array nds_font8x8 --type "unsigned char" $(SRCROOT)/nds/nds_font8x8.png > $@

CLEAN += nds/kbd_graphics.c nds/nds_font8x8.c

############################################################################
# Distribution creation

.PHONY: dist-nds
dist-nds: build-bin doc/xroar.pdf
	mkdir $(DISTNAME)-nds
	cp COPYING.GPL ChangeLog README doc/xroar.pdf xroar.nds xroar.ds.gba $(DISTNAME)-nds/
	rm -f ../$(DISTNAME)-nds.zip
	zip -r ../$(DISTNAME)-nds.zip $(DISTNAME)-nds
	rm -rf $(DISTNAME)-nds/
