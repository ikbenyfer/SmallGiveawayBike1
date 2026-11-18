# Theos-makefile - Small Giveaway Bike 1 (iOS Legacy Edition)
# iphone:clang:<SDK>:<minimale iOS>. SDK 14.5 heeft nog armv7; minimaal iOS 9.0 (iPad 2).
TARGET := iphone:clang:14.5:9.0
ARCHS := armv7

include $(THEOS)/makefiles/common.mk

APPLICATION_NAME = SmallGiveawayBike1

SmallGiveawayBike1_FILES = $(wildcard Game/*.c) $(wildcard Render/*.c) $(wildcard App/*.m)
SmallGiveawayBike1_FRAMEWORKS = UIKit Foundation CoreGraphics QuartzCore GLKit OpenGLES Security
SmallGiveawayBike1_CFLAGS = -fobjc-arc -O2 -DGLES_SILENCE_DEPRECATION -Wno-error -Wno-deprecated-declarations \
	-Werror=unguarded-availability -Werror=unguarded-availability-new -Werror=implicit-function-declaration \
	-Werror=incompatible-pointer-types -Werror=return-type

include $(THEOS_MAKE_PATH)/application.mk
