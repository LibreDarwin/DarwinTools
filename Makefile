# Copyright (C) 2026, LibreDarwin
# SPDX-License-Identifier: BSD-3-Clause
# Build system for sw_vers and startupfiletool.
#
# Build layout: every artifact lives under build/; final tools go to
# build/release/ or build/debug/ per CONFIG.
#
# Portable to both GNU make and BSD make (bmake): no pattern rules, no
# ifeq/ifdef/.if conditionals and no $(if)/$(shell) functions.  Per-config
# flags come from make/<CONFIG>.mk so both make variants behave identically.

CONFIG ?= release
SDK    ?= /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
CC     := /Users/sunneva/xnuports-root/devel/xcode-tools/build/release/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang

-include make/$(CONFIG).mk

BUILD_DIR := build/$(CONFIG)
OBJDIR    := $(BUILD_DIR)/obj

CFLAGS := $(OPT) -std=c11 -D_DARWIN_C_SOURCE -isysroot "$(SDK)" -Wall -Wextra

PREFIX  ?= /usr/local
DESTDIR ?=

SW_VERS          := $(BUILD_DIR)/sw_vers
SW_VERS_OBJS     := $(OBJDIR)/sw_vers.o
SW_VERS_PRIVATE  := src/sw_vers/libcfprivate.tbd

SFT              := $(BUILD_DIR)/startupfiletool
SFT_OBJS         := $(OBJDIR)/startupfiletool.o

all: $(SW_VERS) $(SFT)

$(SW_VERS): $(SW_VERS_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(SW_VERS_OBJS) $(SW_VERS_PRIVATE) \
	    -framework CoreFoundation

# sw_vers reaches for two private CF entry points (_CFCopyServerVersionDictionary
# and _kCFSystemVersionProductNameKey and friends) that CFPriv.h would have
# declared, but CFPriv.h is not in the SDK.  cfpriv.h declares them by hand and
# libcfprivate.tbd tells the linker they exist in the real CoreFoundation; both
# live next to the source.
$(OBJDIR)/sw_vers.o: src/sw_vers/sw_vers.c src/sw_vers/cfpriv.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ src/sw_vers/sw_vers.c

$(SFT): $(SFT_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(SFT_OBJS)

# startupfiletool talks to an HFS+ volume header directly, so it needs the
# on-disk format definitions from the SDK's hfs/hfs_format.h and the
# big-endian swap helpers from libkern/OSByteOrder.h.
$(OBJDIR)/startupfiletool.o: src/startupfiletool/startupfiletool.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ src/startupfiletool/startupfiletool.c

install: all
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/sbin
	install -m 0755 $(SW_VERS) $(DESTDIR)$(PREFIX)/bin/sw_vers
	install -m 0755 $(SFT) $(DESTDIR)$(PREFIX)/sbin/startupfiletool
	install -d $(DESTDIR)/System/Library/CoreServices
	install -m 0644 src/SystemVersion.plist $(DESTDIR)/System/Library/CoreServices/SystemVersion.plist

clean:
	rm -rf build

.PHONY: all install clean
