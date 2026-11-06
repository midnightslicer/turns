QT += core gui qml quick quickcontrols2 dbus

CONFIG += c++17 release
TARGET = turns
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/filepicker.h \
    src/portalfilepicker.h \
    src/shuffler.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/portalfilepicker.cpp \
    src/shuffler.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
