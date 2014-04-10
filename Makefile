# Makefile for XRoar.
# You need to run 'configure' first.

# Run with "VERBOSE=1" for normal output.

SUBDIRS = portalib src doc
DIST_SUBDIRS = portalib src doc tools

.PHONY: all
all:
	@$(foreach dir, $(SUBDIRS), $(MAKE) -C $(dir) $@ && ) true

.PHONY: profile-generate profile-use
profile-generate profile-use:
	@$(foreach dir, $(SUBDIRS), $(MAKE) -C $(dir) $@ && ) true

-include config.mak

SRCROOT ?= $(dir $(lastword $(MAKEFILE_LIST)))

include $(SRCROOT)/common.mak

############################################################################
# Install

.PHONY: install
install:
	@$(foreach dir, $(SUBDIRS), $(MAKE) -C $(dir) $@ && ) true

.PHONY: uninstall
uninstall:
	@$(foreach dir, $(SUBDIRS), $(MAKE) -C $(dir) $@ && ) true

############################################################################
# Specific targets

.PHONY: tools/font2c
tools/font2c:
	$(MAKE) -C tools font2c

.PHONY: portalib/libporta.a
portalib/libporta.a:
	$(MAKE) -C portalib libporta.a

.PHONY: src/xroar$(EXEEXT)
src/xroar$(EXEEXT):
	$(MAKE) -C src xroar$(EXEEXT)

.PHONY: doc/xroar.info
doc/xroar.info:
	$(MAKE) -C doc xroar.info

.PHONY: doc/xroar.pdf
doc/xroar.pdf:
	$(MAKE) -C doc xroar.pdf

.PHONY: doc/xroar.html
doc/xroar.html:
	$(MAKE) -C doc xroar.html

.PHONY: doc/xroar.txt
doc/xroar.txt:
	$(MAKE) -C doc xroar.txt

############################################################################
# Distribution

.PHONY: dist
dist:
	git archive --format=tar --output=$(distdir).tar --prefix=$(distdir)/ HEAD
	gzip -f9 $(distdir).tar

.PHONY: dist-w64
dist-w64: all doc/xroar.pdf
	mkdir $(distdir)-w64
	cp $(SRCROOT)/COPYING.GPL $(SRCROOT)/ChangeLog $(SRCROOT)/README doc/xroar.pdf src/xroar.exe $(prefix)/bin/SDL.dll $(prefix)/bin/libsndfile-1.dll $(distdir)-w64/
	cp $(SRCROOT)/COPYING.LGPL-2.1 $(distdir)-w64/COPYING.LGPL-2.1
	$(TOOL_PREFIX)strip $(distdir)-w64/xroar.exe
	$(TOOL_PREFIX)strip $(distdir)-w64/SDL.dll
	$(TOOL_PREFIX)strip $(distdir)-w64/libsndfile-1.dll
	rm -f ../$(distdir)-w64.zip
	zip -r ../$(distdir)-w64.zip $(distdir)-w64
	rm -rf $(distdir)-w64/

.PHONY: dist-w32
dist-w32: all doc/xroar.pdf
	mkdir $(distdir)-w32
	cp $(SRCROOT)/COPYING.GPL $(SRCROOT)/ChangeLog $(SRCROOT)/README doc/xroar.pdf src/xroar.exe $(prefix)/bin/SDL.dll $(prefix)/bin/libsndfile-1.dll $(distdir)-w32/
	cp $(SRCROOT)/COPYING.LGPL-2.1 $(distdir)-w32/COPYING.LGPL-2.1
	$(TOOL_PREFIX)strip $(distdir)-w32/xroar.exe
	$(TOOL_PREFIX)strip $(distdir)-w32/SDL.dll
	$(TOOL_PREFIX)strip $(distdir)-w32/libsndfile-1.dll
	rm -f ../$(distdir)-w32.zip
	zip -r ../$(distdir)-w32.zip $(distdir)-w32
	rm -rf $(distdir)-w32/

.PHONY: dist-macosx dist-macos
dist-macosx dist-macos: all doc/xroar.pdf
	mkdir XRoar-$(VERSION)
	mkdir -p XRoar-$(VERSION)/XRoar.app/Contents/MacOS XRoar-$(VERSION)/XRoar.app/Contents/Frameworks XRoar-$(VERSION)/XRoar.app/Contents/Resources
	cp src/xroar XRoar-$(VERSION)/XRoar.app/Contents/MacOS/
	cp /usr/local/lib/libSDL-1.2.0.dylib XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/
	chmod 0644 XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/libSDL-1.2.0.dylib
	install_name_tool -change /usr/local/lib/libSDL-1.2.0.dylib @executable_path/../Frameworks/libSDL-1.2.0.dylib XRoar-$(VERSION)/XRoar.app/Contents/MacOS/xroar
	cp /usr/local/lib/libsndfile.1.dylib XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/
	chmod 0644 XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/libsndfile.1.dylib
	install_name_tool -change /usr/local/lib/libsndfile.1.dylib @executable_path/../Frameworks/libsndfile.1.dylib XRoar-$(VERSION)/XRoar.app/Contents/MacOS/xroar
	strip XRoar-$(VERSION)/XRoar.app/Contents/MacOS/xroar
	strip -x XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/libSDL-1.2.0.dylib
	strip -x XRoar-$(VERSION)/XRoar.app/Contents/Frameworks/libsndfile.1.dylib
	sed -e "s!@VERSION@!$(VERSION)!g" $(SRCROOT)/src/macosx/Info.plist.in > XRoar-$(VERSION)/XRoar.app/Contents/Info.plist
	cp $(SRCROOT)/src/macosx/xroar.icns XRoar-$(VERSION)/XRoar.app/Contents/Resources/
	cp $(SRCROOT)/README $(SRCROOT)/COPYING.GPL $(SRCROOT)/ChangeLog doc/xroar.pdf XRoar-$(VERSION)/
	cp $(SRCROOT)/COPYING.LGPL-2.1 XRoar-$(VERSION)/COPYING.LGPL-2.1
	chmod -R o+rX,g+rX XRoar-$(VERSION)/
	hdiutil create -srcfolder XRoar-$(VERSION) -uid 99 -gid 99 ../XRoar-$(VERSION).dmg
	rm -rf XRoar-$(VERSION)/

.PHONY: debuild
debuild: dist
	-cd ..; rm -rf $(distdir)/ $(distdir).orig/
	cd ..; mv $(distdir).tar.gz xroar_$(VERSION).orig.tar.gz
	cd ..; tar xfz xroar_$(VERSION).orig.tar.gz
	rsync -axH debian --exclude='debian/.git/' --exclude='debian/_darcs/' ../$(distdir)/
	cd ../$(distdir); debuild

############################################################################
# Clean-up, etc.

.PHONY: clean
clean:
	@$(foreach dir, $(DIST_SUBDIRS), $(MAKE) -C $(dir) $@ && ) true

.PHONY: profile-clean
profile-clean:
	@$(foreach dir, $(DIST_SUBDIRS), $(MAKE) -C $(dir) $@ && ) true

.PHONY: distclean
distclean:
	@$(foreach dir, $(SUBDIRS), $(MAKE) -C $(dir) $@ && ) true
	rm -f config.h config.mak config.log

.PHONY: depend
depend:
	@$(foreach dir, $(SUBDIRS), $(MAKE) -C $(dir) $@ && ) true
