#-------------------------------------------------
#
# Project created by QtCreator 2024-11-06T20:09:05
#
#-------------------------------------------------

QT       += core gui
QT       += serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

TARGET = PinHZ
TEMPLATE = app
RC_ICONS = ./icon/MI.ico

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp \
        widget.cpp \
    Hex2Dec.cpp \
    FormFillItem.cpp \
    FormDataLog.cpp \
    AppCalcCRC.cpp \
    FormCRCConf.cpp

HEADERS += \
        widget.h \
    Hex2Dec.h \
    FormFillItem.h \
    FormDataLog.h \
    AppCalcCRC.h \
    FormCRCConf.h

FORMS += \
        widget.ui \
    FormFillItem.ui \
    FormDataLog.ui \
    FormCRCConf.ui

RESOURCES += \
    resources.qrc
