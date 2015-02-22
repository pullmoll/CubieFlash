#-------------------------------------------------
#
# Project created by QtCreator 2015-02-21T12:07:58
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = CubieFlasher
TEMPLATE = app
VERSION = 0.1.1

win32:DEFINES += __func__=__FUNCTION__
unix:DEFINES += __func__=__PRETTY_FUNCTION__

INCLUDEPATH += /usr/include/libusb-1.0

SOURCES += main.cpp\
	cubieflasher.cpp \
    usbfel.cpp \
    flasher.cpp \
    about.cpp

HEADERS  += cubieflasher.h \
    usbfel.h \
    flasher.h \
    about.h

FORMS    += cubieflasher.ui \
    about.ui

RESOURCES += \
    cubieflasher.qrc

LIBS += -lusb-1.0

OTHER_FILES += \
    data/fes_1-1.asm \
    img/cubieflasher.png
