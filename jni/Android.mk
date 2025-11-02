LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(CLEAR_VARS)
LOCAL_MODULE    := sagar


LOCAL_CFLAGS := -Wno-error=format-security -fvisibility=hidden -ffunction-sections -fdata-sections -w
LOCAL_CFLAGS += -fno-rtti -fno-exceptions -fpermissive 

LOCAL_CPPFLAGS += -mllvm -sub -mllvm -sub_loop=2
LOCAL_CPPFLAGS += -mllvm -sobf
LOCAL_CPPFLAGS += -mllvm -split -mllvm -split_num=2
LOCAL_CPPFLAGS += -mllvm -bcf -mllvm -bcf_loop=2 -mllvm -bcf_prob=100
LOCAL_CPPFLAGS += -mllvm -fla

LOCAL_CPPFLAGS := -Wno-error=format-security -fvisibility=hidden -ffunction-sections -fdata-sections -w -Werror -s -std=c++17
LOCAL_CPPFLAGS += -Wno-error=c++11-narrowing -fms-extensions -fno-rtti -fno-exceptions -fpermissive
LOCAL_LDFLAGS += -Wl,--gc-sections,--strip-all, -llog
LOCAL_ARM_MODE := arm

LOCAL_C_INCLUDES += $(LOCAL_PATH)

LOCAL_SRC_FILES := Main.cpp \
	Tools/android_native_app_glue.c \
    Encryption/oxorany.cpp \
    Tools/Tools.cpp \
    Tools/fake_dlfcn.cpp \

include $(BUILD_SHARED_LIBRARY)
