QT += core gui quick quickcontrols2 testlib dbus
CONFIG += c++17 testcase
TARGET = turns_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/filepicker.h ../src/shuffler.h ../src/theme.h ../src/translator.h
SOURCES += turns_tests.cpp ../src/backend.cpp ../src/shuffler.cpp ../src/theme.cpp ../src/translator.cpp

DEFINES += TURNS_I18N=\\\"$$PWD/../src/i18n\\\"
