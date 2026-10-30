# PhotoCraft Classic build.
#
#   make            Windows binaries (cross-compiled with i686-w64-mingw32)
#   make test       native unit tests of the portable engine + codecs
#   make check      import audit against the Windows 95 API allowlist
#   make msi        Windows Installer package (wixl / msitools)
#   make dist       everything a release needs, in dist/
#
# The Windows binaries never link msvcrt.dll or any other C runtime:
# src/rt/pcrt.c supplies the C library on top of KERNEL32 (see
# docs/architecture.md, "Runtime").

VERSION    := $(shell cat VERSION)
CROSS      ?= i686-w64-mingw32-
CC_WIN     := $(CROSS)gcc
WINDRES    := $(CROSS)windres
OBJDUMP    := $(CROSS)objdump
CC_NATIVE  ?= cc
PYTHON     ?= python3

B          := build
BW         := $(B)/win
BN         := $(B)/native

COMMON_CFLAGS := -std=gnu99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare \
                 -Wno-missing-field-initializers -Isrc -Isrc/rt -Ithird_party \
                 -DPC_VERSION=\"$(VERSION)\"

# i486 baseline, x87 only: Windows 95 runs on 386DX/486/Pentium-class CPUs
# and never saves SSE state.
WIN_CFLAGS := $(COMMON_CFLAGS) -march=i486 -mtune=pentium -mno-sse -mno-mmx -mfpmath=387 \
              -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident \
              -ffunction-sections -fdata-sections -DWINVER=0x0400 -D_WIN32_WINNT=0x0400 \
              -D_WIN32_IE=0x0300 -DPC_WIN32=1
RT_CFLAGS  := $(WIN_CFLAGS) -fno-builtin -fno-tree-loop-distribute-patterns

WIN_LDFLAGS := -nostdlib -nostartfiles -Wl,-e,_pc_entry -Wl,--gc-sections -s \
               -Wl,--major-os-version,4 -Wl,--minor-os-version,0 \
               -Wl,--major-subsystem-version,4 -Wl,--minor-subsystem-version,0 \
               -Wl,--disable-dynamicbase -Wl,--disable-nxcompat \
               -Wl,--disable-reloc-section
WIN_GUI    := -Wl,--subsystem,windows:4.0

NATIVE_CFLAGS := $(COMMON_CFLAGS) -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer

RT_SRC     := src/rt/pcrt.c
FMT_SRC    := src/rt/pcfmt.c
CORE_SRC   := $(wildcard src/core/*.c)
IO_SRC     := $(wildcard src/io/*.c)
UI_SRC     := $(wildcard src/ui/*.c)
UPD_SRC    := $(wildcard src/updater/*.c)
SETUP_SRC  := $(wildcard src/setup/*.c)
TEST_SRC   := $(wildcard tests/*.c)

# BearSSL: the TLS 1.2 client used by the updater (bundled so the updater
# does not depend on the outdated Schannel/WinINet of old Windows).
BEARSSL_SRC := $(shell find third_party/bearssl/src -name '*.c' 2>/dev/null)
MONO_SRC   := third_party/monocypher/monocypher.c third_party/monocypher/optional/monocypher-ed25519.c

APP_EXE    := $(BW)/PhotoCraftClassic.exe
UPD_EXE    := $(BW)/PCUpdate.exe
SETUP_EXE  := $(BW)/Setup.exe

.PHONY: all test check msi dist clean

all: $(APP_EXE) $(UPD_EXE) $(SETUP_EXE)

obj_win = $(patsubst %.c,$(BW)/obj/%.o,$(1))

$(BW)/obj/src/rt/pcrt.o: src/rt/pcrt.c src/rt/pcrt.h
	@mkdir -p $(dir $@)
	$(CC_WIN) $(RT_CFLAGS) -c $< -o $@

$(BW)/obj/third_party/%.o: third_party/%.c
	@mkdir -p $(dir $@)
	$(CC_WIN) $(WIN_CFLAGS) -Wno-all -Wno-extra -Ithird_party/bearssl/inc -Ithird_party/bearssl/src -c $< -o $@

$(BW)/obj/%.o: %.c $(wildcard src/*/*.h)
	@mkdir -p $(dir $@)
	$(CC_WIN) $(WIN_CFLAGS) -Ithird_party/bearssl/inc -c $< -o $@

$(BW)/res/%.o: res/%.rc $(wildcard res/*)
	@mkdir -p $(dir $@)
	$(WINDRES) -DPC_VERSION_STR=\\\"$(VERSION)\\\" -DPC_VERSION_NUM=$(shell echo $(VERSION) | tr . ,),0 -Ires -i $< -o $@

APP_OBJ := $(call obj_win,$(RT_SRC) $(FMT_SRC) $(CORE_SRC) $(IO_SRC) $(UI_SRC)) $(BW)/res/app.o
$(APP_EXE): $(APP_OBJ)
	$(CC_WIN) $(WIN_LDFLAGS) $(WIN_GUI) $^ -o $@ -lgdi32 -luser32 -lkernel32 -ladvapi32 -lshell32 -lwinspool -lgcc

UPD_OBJ := $(call obj_win,$(RT_SRC) $(FMT_SRC) $(UPD_SRC) $(BEARSSL_SRC) $(MONO_SRC)) $(BW)/res/updater.o
$(UPD_EXE): $(UPD_OBJ)
	$(CC_WIN) $(WIN_LDFLAGS) $(WIN_GUI) $^ -o $@ -lgdi32 -luser32 -lkernel32 -ladvapi32 -lwsock32 -lgcc

SETUP_OBJ := $(call obj_win,$(RT_SRC) $(FMT_SRC) $(SETUP_SRC)) $(BW)/res/setup.o
$(SETUP_EXE): $(SETUP_OBJ)
	$(CC_WIN) $(WIN_LDFLAGS) $(WIN_GUI) $^ -o $@ -lgdi32 -luser32 -lkernel32 -ladvapi32 -lversion -lgcc

# ---- native tests (engine + codecs + updater logic on the host libc) ----
TEST_BIN := $(BN)/pctests
$(TEST_BIN): $(FMT_SRC) $(CORE_SRC) $(IO_SRC) $(TEST_SRC) src/updater/manifest.c $(MONO_SRC) $(wildcard src/*/*.h)
	@mkdir -p $(BN)
	$(CC_NATIVE) $(NATIVE_CFLAGS) $(FMT_SRC) $(CORE_SRC) $(IO_SRC) $(TEST_SRC) src/updater/manifest.c \
		third_party/bearssl/src/hash/sha2small.c third_party/bearssl/src/codec/dec32be.c third_party/bearssl/src/codec/enc32be.c \
		$(MONO_SRC) -Ithird_party/bearssl/inc -Ithird_party/bearssl/src -o $@ -lm

test: $(TEST_BIN)
	$(TEST_BIN)

# Host-side release signing tool (Ed25519 over the update manifest).
SIGN_TOOL := $(BN)/pcsign
$(SIGN_TOOL): tools/pcsign.c $(MONO_SRC)
	@mkdir -p $(BN)
	$(CC_NATIVE) -O2 -Ithird_party $^ -o $@

check: all
	$(PYTHON) tools/check_imports.py --objdump $(OBJDUMP) --allowlist tools/win95-api-allowlist.txt $(APP_EXE) $(UPD_EXE) $(SETUP_EXE)

msi: all
	installer/build-msi.sh $(VERSION)

dist: check msi $(SIGN_TOOL)
	installer/make-dist.sh $(VERSION)

clean:
	rm -rf $(B) dist
