# Nintendo Switch-versie (homebrew .nro) - bouwen met devkitPro (devkitA64 + libnx + switch-portlibs).
# Gebruikt door CI (container devkitpro/devkita64):  make -f Switch/Makefile
# Uitvoer: SmallGiveawayBike1.nro (met romfs: alle plaatjes, geluiden, stemmen en de NL-kaart).
DEVKITPRO ?= /opt/devkitpro
PREFIX    := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-
CC        := $(PREFIX)gcc
PORTLIBS  := $(DEVKITPRO)/portlibs/switch
LIBNX     := $(DEVKITPRO)/libnx
PKGCONF   := $(PORTLIBS)/bin/aarch64-none-elf-pkg-config
VERSION   := $(shell cat VERSION)

ARCH    := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS  := -std=gnu11 -O2 -ffunction-sections $(ARCH) -D__SWITCH__ -I$(LIBNX)/include -I$(PORTLIBS)/include \
           $(shell $(PKGCONF) --cflags sdl2 SDL2_mixer libcurl) -Wno-unused-parameter
LDFLAGS := -specs=$(LIBNX)/switch.specs $(ARCH) -L$(LIBNX)/lib -L$(PORTLIBS)/lib
LIBS    := $(shell $(PKGCONF) --libs SDL2_mixer sdl2 libcurl) -lEGL -lGLESv2 -lglapi -ldrm_nouveau -lz -lnx -lm

SRC := $(wildcard Game/*.c) $(wildcard Render/*.c) Win/main_win.c
OBJ := $(patsubst %.c,switch-build/obj/%.o,$(SRC))

all: SmallGiveawayBike1.nro

switch-build/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

switch-build/SmallGiveawayBike1.elf: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@

switch-build/app.nacp:
	$(DEVKITPRO)/tools/bin/nacptool --create "Giveaway Bike" "Giveaway Bike" "$(VERSION)" $@

SmallGiveawayBike1.nro: switch-build/SmallGiveawayBike1.elf switch-build/app.nacp
	$(DEVKITPRO)/tools/bin/elf2nro $< $@ --icon=switch-build/icon.jpg --nacp=switch-build/app.nacp --romfsdir=switch-build/romfs

clean:
	rm -rf switch-build SmallGiveawayBike1.nro
