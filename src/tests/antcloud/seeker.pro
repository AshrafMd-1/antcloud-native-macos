QT += core gui network qml testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
INCLUDEPATH += ../../app ../../moonlight-common-c/moonlight-common-c/src ../../qmdnsengine ../../qmdnsengine/qmdnsengine/src/include
SOURCES += seeker_test.cpp host_test_services.cpp \
    ../../app/backend/computerseeker.cpp ../../app/backend/computermanager.cpp \
    ../../app/backend/nvcomputer.cpp ../../app/backend/nvhttp.cpp ../../app/backend/nvapp.cpp \
    ../../app/backend/nvaddress.cpp ../../app/backend/nvpairingmanager.cpp ../../app/backend/identitymanager.cpp
HEADERS += ../../app/backend/computerseeker.h ../../app/backend/computermanager.h \
    ../../app/backend/nvhttp.h ../../app/settings/streamingpreferences.h ../../app/settings/compatfetcher.h
SOURCES += $$files($$PWD/../../qmdnsengine/qmdnsengine/src/src/*.cpp)
HEADERS += $$files($$PWD/../../qmdnsengine/qmdnsengine/src/include/qmdnsengine/*.h)
HEADERS += $$files($$PWD/../../qmdnsengine/qmdnsengine/src/src/*.h)
unix:!macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += sdl2 openssl
}
macx {
    INCLUDEPATH += $$PWD/../../libs/mac/include $$PWD/../../libs/mac/include/SDL2
    LIBS += -L$$PWD/../../libs/mac/lib -lssl.3 -lcrypto.3 -lSDL2
    QMAKE_RPATHDIR += $$PWD/../../libs/mac/lib
}
