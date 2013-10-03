# Makefile for XRoar.
# You need to run 'configure' first.

# Run with "VERBOSE=1" for normal output.

.PHONY: all
all: build-bin

.PHONY: profile-generate profile-use
profile-generate profile-use: all

-include ../config.mak

SRCROOT ?= $(dir $(lastword $(MAKEFILE_LIST)))
include $(SRCROOT)/common.mak
SRCROOT := $(SRCROOT)/src

CONFIG_FILES = ../config.h ../config.mak

############################################################################
# Base files

### XRoar

xroar_CLEAN_O =
xroar_CLEAN =
xroar_SOURCES_C =
xroar_SOURCES_CXX =
xroar_SOURCES_OBJC =
xroar_SOURCES_RC =
xroar_C_O =
xroar_CXX_O =
xroar_OBJC_O =
xroar_RC_RES =

xroar_CFLAGS = $(CFLAGS) $(CPPFLAGS) \
	-I.. -I$(CURDIR) -I$(SRCROOT)/../portalib -I$(SRCROOT)/../src \
	$(WARN) \
        -DVERSION=\"$(VERSION)\" \
        -DROMPATH=$(ROMPATH) -DCONFPATH=$(CONFPATH)
xroar_CXXFLAGS = $(CXXFLAGS) $(CPPFLAGS) \
	-I.. -I$(CURDIR) -I$(SRCROOT)/../portalib -I$(SRCROOT)/../src \
	$(WARN) \
        -DVERSION=\"$(VERSION)\" \
        -DROMPATH=$(ROMPATH) -DCONFPATH=$(CONFPATH)
xroar_OBJCFLAGS = $(OBJCFLAGS) $(CPPFLAGS) \
	-I.. -I$(CURDIR) -I$(SRCROOT)/../portalib -I$(SRCROOT)/../src \
	$(WARN) \
        -DVERSION=\"$(VERSION)\" \
        -DROMPATH=$(ROMPATH) -DCONFPATH=$(CONFPATH)
xroar_LDFLAGS = $(LDFLAGS) $(LDLIBS)

xroar_BASE_C = \
	becker.c \
	breakpoint.c \
	cart.c \
	crc16.c \
	crc32.c \
	crclist.c \
	deltados.c \
	dragondos.c \
	events.c \
	fs.c \
	hd6309.c \
	hexs19.c \
	joystick.c \
	keyboard.c \
	logging.c \
	machine.c \
	mc6809.c \
	mc6821.c \
	mc6847.c \
	module.c \
	orch90.c \
	path.c \
	printer.c \
	romlist.c \
	rsdos.c \
	sam.c \
	snapshot.c \
	sound.c \
	tape.c \
	tape_cas.c \
	ui_null.c \
	vdg_bitmaps.c \
	vdg_palette.c \
	vdisk.c \
	vdrive.c \
	vo_null.c \
	wd279x.c \
	xconfig.c \
	xroar.c
xroar_BASE_C_O = $(xroar_BASE_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_BASE_C)
xroar_CLEAN_O += $(xroar_BASE_C_O)
xroar_C_O += $(xroar_BASE_C_O)

# Objects for all Unix-style builds (now the only one):
xroar_unix_C = main_unix.c
xroar_unix_C_O = $(xroar_unix_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_unix_C)
xroar_CLEAN_O += $(xroar_unix_C_O)
xroar_C_O += $(xroar_unix_C_O)

############################################################################
# Options

ifeq ($(opt_zlib),yes)
	xroar_CFLAGS += $(opt_zlib_CFLAGS)
	xroar_LDFLAGS += $(opt_zlib_LDFLAGS)
endif

ifeq ($(opt_glib2),yes)
	xroar_CFLAGS += $(opt_glib2_CFLAGS)
	xroar_LDFLAGS += $(opt_glib2_LDFLAGS)
endif

xroar_gdb_C = gdb.c
xroar_gdb_C_O = $(xroar_gdb_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_gdb_C)
xroar_CLEAN_O += $(xroar_gdb_C_O)
ifeq ($(opt_gdb_target),yes)
	xroar_C_O += $(xroar_gdb_C_O)
endif

ifeq ($(opt_glib2),yes)
	xroar_CFLAGS += $(opt_glib2_CFLAGS)
	xroar_OBJCFLAGS += $(opt_glib2_CFLAGS)
	xroar_LDFLAGS += $(opt_glib2_LDFLAGS)
endif

xroar_gtk2_C = \
	gtk2/drivecontrol.c \
	gtk2/filereq_gtk2.c \
	gtk2/keyboard_gtk2.c \
	gtk2/tapecontrol.c \
	gtk2/ui_gtk2.c
xroar_gtk2_C_O = $(xroar_gtk2_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_gtk2_C)
xroar_CLEAN_O += $(xroar_gtk2_C_O)
ifeq ($(opt_gtk2),yes)
	xroar_C_O += $(xroar_gtk2_C_O)
	xroar_CFLAGS += $(opt_gtk2_CFLAGS)
	xroar_LDFLAGS += $(opt_gtk2_LDFLAGS)
gtk2/drivecontrol.o: gtk2/drivecontrol_glade.h
gtk2/tapecontrol.o: gtk2/tapecontrol_glade.h
gtk2/ui_gtk2.o: gtk2/top_window_glade.h
keyboard_gtk2.o: $(SRCROOT)/gtk2/keyboard_gtk2_mappings.c
endif

xroar_gtkgl_C = gtk2/vo_gtkgl.c
xroar_gtkgl_C_O = $(xroar_gtkgl_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_gtkgl_C)
xroar_CLEAN_O += $(xroar_gtkgl_C_O)
ifeq ($(opt_gtkgl),yes)
	xroar_C_O += $(xroar_gtkgl_C_O)
	xroar_CFLAGS += $(opt_gtkgl_CFLAGS)
	xroar_LDFLAGS += $(opt_gtkgl_LDFLAGS)
endif

xroar_opengl_C = vo_opengl.c
xroar_opengl_C_O = $(xroar_opengl_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_opengl_C)
xroar_CLEAN_O += $(xroar_opengl_C_O)
ifeq ($(opt_opengl),yes)
	xroar_C_O += $(xroar_opengl_C_O)
	xroar_CFLAGS += $(opt_opengl_CFLAGS)
	xroar_LDFLAGS += $(opt_opengl_LDFLAGS)
endif

xroar_sdl_C = \
	sdl/ao_sdl.c \
	sdl/common.c \
	sdl/joystick_sdl.c \
	sdl/keyboard_sdl.c \
	sdl/ui_sdl.c \
	sdl/vo_sdl.c \
	sdl/vo_sdlyuv.c
xroar_sdl_C_O = $(xroar_sdl_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_sdl_C)
xroar_CLEAN_O += $(xroar_sdl_C_O)
ifeq ($(opt_sdl),yes)
	xroar_C_O += $(xroar_sdl_C_O)
	xroar_CFLAGS += $(opt_sdl_CFLAGS)
	xroar_OBJCFLAGS += $(opt_sdl_OBJCFLAGS)
	xroar_LDFLAGS += $(opt_sdl_LDFLAGS)
sdl/keyboard_sdl.o: $(SRCROOT)/sdl/keyboard_sdl_mappings.c
endif

xroar_sdlgl_C = sdl/vo_sdlgl.c
xroar_sdlgl_C_O = $(xroar_sdlgl_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_sdlgl_C)
xroar_CLEAN_O += $(xroar_sdlgl_C_O)
ifeq ($(opt_sdlgl),yes)
	xroar_C_O += $(xroar_sdlgl_C_O)
	xroar_CFLAGS += $(opt_sdlgl_CFLAGS)
	xroar_LDFLAGS += $(opt_sdlgl_LDFLAGS)
endif

xroar_cli_C = filereq_cli.c
xroar_cli_C_O = $(xroar_cli_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_cli_C)
xroar_CLEAN_O += $(xroar_cli_C_O)
ifeq ($(opt_cli),yes)
	xroar_C_O += $(xroar_cli_C_O)
	xroar_CFLAGS += $(opt_cli_CFLAGS)
	xroar_LDFLAGS += $(opt_cli_LDFLAGS)
endif

xroar_cocoa_OBJC = \
	macosx/filereq_cocoa.m \
	macosx/ui_macosx.m
xroar_cocoa_OBJC_O = $(xroar_cocoa_OBJC:.m=.o)
#
xroar_SOURCES_OBJC += $(xroar_cocoa_OBJC)
xroar_CLEAN_O += $(xroar_cocoa_OBJC_O)
ifeq ($(opt_cocoa),yes)
	xroar_OBJC_O += $(xroar_cocoa_OBJC_O)
	xroar_CFLAGS += $(opt_cocoa_CFLAGS)
	xroar_OBJCFLAGS += $(opt_cocoa_OBJCFLAGS)
	xroar_LDFLAGS += $(opt_cocoa_LDFLAGS)
endif

xroar_alsa_C = alsa/ao_alsa.c
xroar_alsa_C_O = $(xroar_alsa_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_alsa_C)
xroar_CLEAN_O += $(xroar_alsa_C_O)
ifeq ($(opt_alsa),yes)
	xroar_C_O += $(xroar_alsa_C_O)
	xroar_CFLAGS += $(opt_alsa_CFLAGS)
	xroar_LDFLAGS += $(opt_alsa_LDFLAGS)
endif

xroar_oss_C = oss/ao_oss.c
xroar_oss_C_O = $(xroar_oss_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_oss_C)
xroar_CLEAN_O += $(xroar_oss_C_O)
ifeq ($(opt_oss),yes)
	xroar_C_O += $(xroar_oss_C_O)
	xroar_CFLAGS += $(opt_oss_CFLAGS)
	xroar_LDFLAGS += $(opt_oss_LDFLAGS)
endif

xroar_pulse_C = pulseaudio/ao_pulse.c
xroar_pulse_C_O = $(xroar_pulse_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_pulse_C)
xroar_CLEAN_O += $(xroar_pulse_C_O)
ifeq ($(opt_pulse),yes)
	xroar_C_O += $(xroar_pulse_C_O)
	xroar_CFLAGS += $(opt_pulse_CFLAGS)
	xroar_LDFLAGS += $(opt_pulse_LDFLAGS)
endif

xroar_sunaudio_C = sunos/ao_sun.c
xroar_sunaudio_C_O = $(xroar_sunaudio_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_sunaudio_C)
xroar_CLEAN_O += $(xroar_sunaudio_C_O)
ifeq ($(opt_sunaudio),yes)
	xroar_C_O += $(xroar_sunaudio_C_O)
	xroar_CFLAGS += $(opt_sunaudio_CFLAGS)
	xroar_LDFLAGS += $(opt_sunaudio_LDFLAGS)
endif

xroar_coreaudio_C = macosx/ao_macosx.c
xroar_coreaudio_C_O = $(xroar_coreaudio_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_coreaudio_C)
xroar_CLEAN_O += $(xroar_coreaudio_C_O)
ifeq ($(opt_coreaudio),yes)
	xroar_C_O += $(xroar_coreaudio_C_O)
	xroar_CFLAGS += $(opt_coreaudio_CFLAGS)
	xroar_LDFLAGS += $(opt_coreaudio_LDFLAGS)
endif

xroar_jack_C = jack/ao_jack.c
xroar_jack_C_O = $(xroar_jack_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_jack_C)
xroar_CLEAN_O += $(xroar_jack_C_O)
ifeq ($(opt_jack),yes)
	xroar_C_O += $(xroar_jack_C_O)
	xroar_CFLAGS += $(opt_jack_CFLAGS)
	xroar_LDFLAGS += $(opt_jack_LDFLAGS)
endif

xroar_sndfile_C = tape_sndfile.c
xroar_sndfile_C_O = $(xroar_sndfile_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_sndfile_C)
xroar_CLEAN_O += $(xroar_sndfile_C_O)
ifeq ($(opt_sndfile),yes)
	xroar_C_O += $(xroar_sndfile_C_O)
	xroar_CFLAGS += $(opt_sndfile_CFLAGS)
	xroar_LDFLAGS += $(opt_sndfile_LDFLAGS)
endif

xroar_nullaudio_C = ao_null.c
xroar_nullaudio_C_O = $(xroar_nullaudio_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_nullaudio_C)
xroar_CLEAN_O += $(xroar_nullaudio_C_O)
ifeq ($(opt_nullaudio),yes)
	xroar_C_O += $(xroar_nullaudio_C_O)
	xroar_CFLAGS += $(opt_nullaudio_CFLAGS)
	xroar_LDFLAGS += $(opt_nullaudio_LDFLAGS)
endif

xroar_linux_joystick_C = linux/joystick_linux.c
xroar_linux_joystick_C_O = $(xroar_linux_joystick_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_linux_joystick_C)
xroar_CLEAN_O += $(xroar_linux_joystick_C_O)
ifeq ($(opt_linux_joystick),yes)
	xroar_C_O += $(xroar_linux_joystick_C_O)
	xroar_CFLAGS += $(opt_linux_joystick_CFLAGS)
	xroar_LDFLAGS += $(opt_linux_joystick_LDFLAGS)
endif

xroar_mingw_C = \
	windows32/ao_windows32.c \
	windows32/common_windows32.c \
	windows32/filereq_windows32.c
xroar_mingw_C_O = $(xroar_mingw_C:.c=.o)
xroar_mingw_RC = windows32/xroar.rc
xroar_mingw_RC_RES = $(xroar_mingw_RC:.rc=.res)
#
xroar_SOURCES_C += $(xroar_mingw_C)
xroar_SOURCES_RC += $(xroar_mingw_RES)
xroar_CLEAN_O += $(xroar_mingw_C_O)
xroar_CLEAN += $(xroar_mingw_RC_RES)
ifeq ($(opt_mingw),yes)
	xroar_C_O += $(xroar_mingw_C_O)
	xroar_RC_RES += $(xroar_mingw_RC_RES)
	xroar_CFLAGS += $(opt_mingw_CFLAGS)
	xroar_LDFLAGS += $(opt_mingw_LDFLAGS)
endif

xroar_trace_C = mc6809_trace.c hd6309_trace.c
xroar_trace_C_O = $(xroar_trace_C:.c=.o)
#
xroar_SOURCES_C += $(xroar_trace_C)
xroar_CLEAN_O += $(xroar_trace_C_O)
ifeq ($(opt_trace),yes)
	xroar_C_O += $(xroar_trace_C_O)
	xroar_CFLAGS += $(opt_trace_CFLAGS)
	xroar_LDFLAGS += $(opt_trace_LDFLAGS)
endif

ifeq ($(opt_pthreads),yes)
	xroar_LDFLAGS += $(opt_pthreads_LDFLAGS)
endif

############################################################################
# Build

### Portalib

../portalib/libporta.a:
	$(MAKE) -C .. portalib/libporta.a

### XRoar

ifeq ($(opt_mingw),yes)
ROMPATH = \":~/Local\ Settings/Application\ Data/XRoar/roms:~/Application\ Data/XRoar/roms\"
CONFPATH = \":~/Local\ Settings/Application\ Data/XRoar:~/Application\ Data/XRoar\"
endif

ifeq ($(opt_coreaudio),yes)
ROMPATH = \"~/Library/XRoar/roms:~/.xroar/roms:$(datadir)/xroar/roms:\"
CONFPATH = \"~/Library/XRoar:~/.xroar:$(sysconfdir):$(datadir)/xroar\"
endif

ifndef ROMPATH
ROMPATH = \"~/.xroar/roms:$(datadir)/xroar/roms:\"
CONFPATH = \"~/.xroar:$(sysconfdir):$(datadir)/xroar\"
endif

xroar_CLEAN += $(xroar_CLEAN_O)

xroar_OBJS = \
	$(xroar_C_O) \
	$(xroar_CXX_O) \
	$(xroar_OBJC_O) \
	$(xroar_RC_RES)

$(xroar_OBJS): $(CONFIG_FILES)

$(xroar_C_O): %.o: $(SRCROOT)/%.c
	$(call do_cc,$@,$(xroar_CFLAGS) -c $<)

$(xroar_CXX_O): %.o: $(SRCROOT)/%.cc
	$(call do_cc,$@,$(xroar_CXXFLAGS) -c $<)

$(xroar_OBJC_O): %.o: $(SRCROOT)/%.m
	$(call do_objc,$@,$(xroar_OBJCFLAGS) -c $<)

$(xroar_RC_RES): %.res: $(SRCROOT)/%.rc
	$(call do_windres,$@,-O coff -DVERSION=$(VERSION) -DVERSION_MAJOR=$(VERSION_MAJOR) -DVERSION_MINOR=$(VERSION_MINOR) -DVERSION_PATCH=$(VERSION_PATCH) -DVERSION_SUBPATCH=$(VERSION_SUBPATCH) $<)

xroar$(EXEEXT): $(xroar_OBJS) ../portalib/libporta.a
	$(call do_ld,$@,$(xroar_OBJS) ../portalib/libporta.a $(xroar_LDFLAGS))

xroar_CLEAN += xroar xroar.exe

.PHONY: build-bin
build-bin: xroar$(EXEEXT)

############################################################################
# Install

.PHONY: install
install: install-bin

.PHONY: install-bin
install-bin: build-bin
	$(INSTALL_DIR) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) xroar$(EXEEXT) $(DESTDIR)$(bindir)

############################################################################
# Generated dependencies

# font2c

.SECONDARY: ../tools/font2c
../tools/font2c:
	$(MAKE) -C .. tools/font2c

#

vdg_bitmaps.h: ../tools/font2c | $(SRCROOT)/font-6847.png $(SRCROOT)/font-6847t1.png
	../tools/font2c --header --array font_6847 --type "unsigned char" --vdg $(SRCROOT)/font-6847.png > $@
	../tools/font2c --header --array font_6847t1 --type "unsigned char" --vdgt1 $(SRCROOT)/font-6847t1.png >> $@

vdg_bitmaps.c: ../tools/font2c | $(SRCROOT)/font-6847.png
	../tools/font2c --array font_6847 --type "unsigned char" --vdg $(SRCROOT)/font-6847.png > $@
	../tools/font2c --array font_6847t1 --type "unsigned char" --vdgt1 $(SRCROOT)/font-6847t1.png >> $@

#

%.h: %.glade
	echo "static const gchar *$(@:%.h=%) =" | sed 's/\*.*\//\*/'> $@
	sed 's/"/'\''/g;s/^\( *\)/\1"/;s/$$/"/;' $< >> $@
	echo ";" >> $@

############################################################################
# Clean-up, etc.

.PHONY: clean
clean:
	rm -f $(xroar_CLEAN)

.PHONY: profile-clean
profile-clean:
	rm -f $(xroar_CLEAN_O:.o=.gcda)
	rm -f $(xroar_CLEAN_O:.o=.gcno)

.PHONY: distclean
distclean: clean profile-clean

.PHONY: depend
depend:
	@touch .deps.mak
	makedepend -f .deps.mak -Y $(xroar_SOURCES_C) 2> /dev/null

-include .deps.mak
