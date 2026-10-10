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
    LIBS += -L$$PWD/../../libs/mac/lib -lSDL2 -lSDL2_ttf
}

macx {
    QMAKE_RPATHDIR += $$PWD/../../libs/mac/lib
}
