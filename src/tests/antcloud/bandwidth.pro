QT += core testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
INCLUDEPATH += ../../app/streaming
SOURCES += bandwidth_test.cpp ../../app/streaming/bandwidth.cpp
