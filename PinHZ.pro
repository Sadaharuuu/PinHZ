QT += core gui widgets serialport network printsupport
CONFIG += c++11
TARGET = PinHZ
VERSION = 3.0.0
TEMPLATE = app
DEFINES += QT_DEPRECATED_WARNINGS
RC_ICONS = res/icon/Sadaharu.ico
INCLUDEPATH += $$PWD src/app src/forms

SOURCES += \
    main.cpp \
    widget.cpp \
    src/app/AppCalcCheck.cpp \
    src/app/AppCalcCRC.cpp \
    src/app/AppCalcExpression.cpp \
    src/app/AppFileTools.cpp \
    src/app/AppItemValue.cpp \
    src/app/AppNavigation.cpp \
    src/app/AppTheme.cpp \
    src/app/AppUiLayout.cpp \
    src/app/AppWindowFrame.cpp \
    src/app/Hex2Dec.cpp \
    src/forms/FormCheckPanel.cpp \
    src/forms/FormCheckTools.cpp \
    src/forms/FormCRCConf.cpp \
    src/forms/FormDataCheck.cpp \
    src/forms/FormDataLog.cpp \
    src/forms/FormFileCheck.cpp \
    src/forms/FormFileRange.cpp \
    src/forms/FormFillItem.cpp \
    src/forms/FormHex2ASCII.cpp \
    src/forms/FormHexSplit.cpp \
    src/forms/FormItemType.cpp \
    src/forms/FormNavigation.cpp \
    src/forms/FormPinHZ.cpp \
    src/forms/FormStopwatch.cpp \
    src/forms/FormStopwatchDial.cpp \
    src/forms/FormTableImport.cpp \
    src/forms/FormTimeConvert.cpp \
    src/forms/FormTools.cpp \
    third_party/blake2/blake2s-ref.c \
    third_party/blake2/blake2sp-ref.c \
    third_party/blake2/blake2b-ref.c \
    third_party/xxhash/xxhash.c

HEADERS += \
    widget.h \
    src/app/AppCalcCheck.h \
    src/app/AppCalcCRC.h \
    src/app/AppCalcExpression.h \
    src/app/AppFileTools.h \
    src/app/AppItemValue.h \
    src/app/AppNavigation.h \
    src/app/AppTheme.h \
    src/app/AppUiLayout.h \
    src/app/AppWindowFrame.h \
    src/app/CommonDefine.h \
    src/app/Hex2Dec.h \
    src/forms/FormCheckPanel.h \
    src/forms/FormCheckTools.h \
    src/forms/FormCRCConf.h \
    src/forms/FormDataCheck.h \
    src/forms/FormDataLog.h \
    src/forms/FormFileCheck.h \
    src/forms/FormFileRange.h \
    src/forms/FormFillItem.h \
    src/forms/FormHex2ASCII.h \
    src/forms/FormHexSplit.h \
    src/forms/FormItemType.h \
    src/forms/FormNavigation.h \
    src/forms/FormPinHZ.h \
    src/forms/FormStopwatch.h \
    src/forms/FormStopwatchDial.h \
    src/forms/FormTableImport.h \
    src/forms/FormTimeConvert.h \
    src/forms/FormTools.h \
    third_party/blake2/blake2.h \
    third_party/blake2/blake2-impl.h \
    third_party/xxhash/xxhash.h

FORMS += \
    widget.ui \
    src/forms/FormCheckPanel.ui \
    src/forms/FormCheckTools.ui \
    src/forms/FormCRCConf.ui \
    src/forms/FormDataCheck.ui \
    src/forms/FormDataLog.ui \
    src/forms/FormFileCheck.ui \
    src/forms/FormFileRange.ui \
    src/forms/FormFillItem.ui \
    src/forms/FormFrameSettings.ui \
    src/forms/FormHex2ASCII.ui \
    src/forms/FormHexSplit.ui \
    src/forms/FormItemType.ui \
    src/forms/FormNavigation.ui \
    src/forms/FormPinHZ.ui \
    src/forms/FormStopwatch.ui \
    src/forms/FormTableImport.ui \
    src/forms/FormTimeConvert.ui \
    src/forms/FormTools.ui

RESOURCES += res/resources.qrc

DISTFILES += \
    .clang-format \
    third_party/blake2/COPYING \
    third_party/blake2/README.md \
    third_party/xxhash/LICENSE \
    third_party/xxhash/README.md

DISTFILES += LICENSE
