# De game zelf (Game/, Render/ en Win/main_win.c met __ANDROID__)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := main
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../SDL/include $(LOCAL_PATH)/../SDL_mixer/include
LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%,%,$(wildcard $(LOCAL_PATH)/Game/*.c) $(wildcard $(LOCAL_PATH)/Render/*.c)) Win/main_win.c
LOCAL_CFLAGS := -std=gnu99 -O2 -DNDEBUG -Wall -Wno-unused-parameter -Wno-misleading-indentation -Wno-unused-function
LOCAL_SHARED_LIBRARIES := SDL2 SDL2_mixer
LOCAL_LDLIBS := -lGLESv2 -llog -lz -landroid
include $(BUILD_SHARED_LIBRARY)
