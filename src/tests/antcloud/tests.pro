QT += core network websockets testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
DEFINES += ANTCLOUD_TESTING
INCLUDEPATH += ../../app
SOURCES += provider_test.cpp ../../app/antcloud/providerworker.cpp ../../app/antcloud/tokenstore.cpp
HEADERS += ../../app/antcloud/providerworker.h
unix:!macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += sdl2
}
macx {
    INCLUDEPATH += $$PWD/../../libs/mac/include/SDL2
    LIBS += -L$$PWD/../../libs/mac/lib -lSDL2 -framework Security
}

macx {
    QMAKE_RPATHDIR += $$PWD/../../libs/mac/lib
}
