SUB_PATH := external/zxing

LOCAL_SRC_FILES += \
	$(SUB_PATH)/qrcode/QRCodecMode.cpp \
	$(SUB_PATH)/qrcode/QREncoder.cpp \
	$(SUB_PATH)/qrcode/QRErrorCorrectionLevel.cpp \
	$(SUB_PATH)/qrcode/QRFormatInformation.cpp \
	$(SUB_PATH)/qrcode/QRMaskUtil.cpp \
	$(SUB_PATH)/qrcode/QRMatrixUtil.cpp \
	$(SUB_PATH)/qrcode/QRVersion.cpp \
	$(SUB_PATH)/qrcode/QRWriter.cpp \
	$(SUB_PATH)/textcodec/Big5MapTable.cpp \
	$(SUB_PATH)/textcodec/Big5TextEncoder.cpp \
	$(SUB_PATH)/textcodec/GBTextEncoder.cpp \
	$(SUB_PATH)/textcodec/JPTextEncoder.cpp \
	$(SUB_PATH)/textcodec/KRHangulMapping.cpp \
	$(SUB_PATH)/textcodec/KRTextEncoder.cpp \
	$(SUB_PATH)/BarcodeFormat.cpp \
	$(SUB_PATH)/BitArray.cpp \
	$(SUB_PATH)/BitMatrix.cpp \
	$(SUB_PATH)/CharacterSetECI.cpp \
	$(SUB_PATH)/GenericGF.cpp \
	$(SUB_PATH)/GenericGFPoly.cpp \
	$(SUB_PATH)/MultiFormatWriter.cpp \
	$(SUB_PATH)/ReedSolomonEncoder.cpp \
	$(SUB_PATH)/TextEncoder.cpp \
	$(SUB_PATH)/TextUtfEncoding.cpp