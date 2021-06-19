SUB_PATH := $(WEBRTC_SUBPATH)/sdk/android

LOCAL_SRC_FILES += \
    $(SUB_PATH)/src/jni/androidmediadecoder_kos.cc \
    $(SUB_PATH)/src/jni/androidmediaencoder_kos.cc \
    $(SUB_PATH)/src/jni/mediacodecvideodecoder_l_kos.cc \
    $(SUB_PATH)/src/jni/mediacodecvideoencoder_l_kos.cc