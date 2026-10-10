QT += core testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
INCLUDEPATH += ../../app ../../app/streaming/input
SOURCES += controls_test.cpp ../../app/streaming/video/overlaymanager.cpp
unix:!macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += sdl2 SDL2_ttf
}
macx {
    INCLUDEPATH += $$PWD/../../libs/mac/include $$PWD/../../libs/mac/include/SDL2
    SOURCES += appkit_stats_test.mm
    HEADERS += appkit_stats_test.h ../../app/streaming/video/ffmpeg-renderers/antcloudstatstextfield.h
    LIBS += -L$$PWD/../../libs/mac/lib -lSDL2 -lSDL2_ttf -framework AppKit
}

macx {
    QMAKE_RPATHDIR += $$PWD/../../libs/mac/lib
}
