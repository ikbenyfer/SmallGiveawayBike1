# Meta Quest-versie: libmain.so (NativeActivity) + de OpenXR-loader (uit de Khronos-AAR, zie tools/quest_prepare.sh)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := openxr_loader
LOCAL_SRC_FILES := openxr/lib/$(TARGET_ARCH_ABI)/libopenxr_loader.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/openxr/include
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := main
LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%,%,$(wildcard $(LOCAL_PATH)/src/Game/*.c) $(wildcard $(LOCAL_PATH)/src/Render/*.c)) src/Quest/main_quest.c
LOCAL_CFLAGS := -O2 -DSGB_QUEST -Wno-unused-parameter -Wno-unused-function
LOCAL_LDLIBS := -lGLESv3 -lEGL -llog -landroid -lz
LOCAL_LDFLAGS := -u ANativeActivity_onCreate
LOCAL_STATIC_LIBRARIES := android_native_app_glue
LOCAL_SHARED_LIBRARIES := openxr_loader
include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
