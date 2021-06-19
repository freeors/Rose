SUB_PATH := external/protobuf
FULL_SUB_PATH := $(LOCAL_PATH)/external/protobuf

LOCAL_SRC_FILES += \
	$(subst $(FULL_SUB_PATH),$(SUB_PATH), \
	$(wildcard $(FULL_SUB_PATH)/src/google/protobuf/io/*.cc) \
	$(wildcard $(FULL_SUB_PATH)/src/google/protobuf/stubs/*.cc) \
	$(wildcard $(FULL_SUB_PATH)/src/google/protobuf/*.cc))
