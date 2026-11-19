# Bovenste ndk-build-bestand: SDL2, SDL2_mixer (alleen ingebouwde decoders) en de game (src/)
LOCAL_PATH := $(call my-dir)
SUPPORT_WAVPACK := false
SUPPORT_MID_TIMIDITY := false
SUPPORT_MOD_XMP := false
SUPPORT_OGG := false
SUPPORT_FLAC_LIBFLAC := false
SUPPORT_MP3_MPG123 := false
include $(call all-subdir-makefiles)
