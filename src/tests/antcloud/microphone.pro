QT += core network testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
INCLUDEPATH += ../../app
SOURCES += microphone_test.cpp ../../app/antcloud/microphone.cpp ../../app/antcloud/microphoneaudio.cpp
HEADERS += ../../app/antcloud/microphone.h ../../app/antcloud/microphoneaudio.h ../../app/antcloud/microphonecapture.h
unix:!macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += sdl2
}
macx {
    INCLUDEPATH += $$PWD/../../libs/mac/include $$PWD/../../libs/mac/include/SDL2
    LIBS += -L$$PWD/../../libs/mac/lib -lSDL2
    QMAKE_RPATHDIR += $$PWD/../../libs/mac/lib
}
