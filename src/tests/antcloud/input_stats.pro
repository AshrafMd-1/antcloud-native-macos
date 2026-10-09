QT += core
CONFIG += console c++17
CONFIG -= app_bundle
INCLUDEPATH += ../../app ../../app/streaming/input ../../app/streaming/video
SOURCES += input_stats_test.cpp
unix:!macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += sdl2
}
macx {
    INCLUDEPATH += $$PWD/../../libs/mac/include/SDL2
}
