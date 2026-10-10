include(app.pro)

TARGET = settings
QT += testlib
CONFIG += console testcase
CONFIG -= app_bundle qtquickcompiler
DEFINES += ANTCLOUD_TESTING

SOURCES -= main.cpp
SOURCES += ../tests/antcloud/settings_test.cpp
RESOURCES =
