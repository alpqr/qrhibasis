TEMPLATE = app

QT += gui-private

SOURCES = \
    qrhibasis.cpp \
    transcoder/basisu_transcoder.cpp

HEADERS = \
    transcoder/basisu_transcoder.h

DEFINES += BASISD_SUPPORT_BC7=0 BASISD_SUPPORT_PVRTC1=0 BASISD_SUPPORT_PVRTC2=0 BASISD_SUPPORT_FXT1=0

RESOURCES = \
    qrhibasis.qrc
