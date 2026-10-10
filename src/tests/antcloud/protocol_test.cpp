#include "antcloud/protocol.h"
#include <QCoreApplication>
#include <cassert>
#include <QSettings>
#include <QTemporaryDir>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    using namespace AntCloud;
    auto mapping=savedModifierMapping({});
    assert(mapping.target(0)==1 && mapping.target(1)==3 && mapping.target(2)==2);
    assert(mapping.assign(0,3) && mapping.target(1)==0);
    assert(mapping.assign(2,3) && mapping.target(0)==0 && mapping.target(2)==3);
    assert(mapping.assign(2,0) && mapping.target(0)==0 && mapping.target(1)==0);
    assert(!mapping.assign(-1,1) && !mapping.assign(3,1) && !mapping.assign(0,4));
    auto invalid=savedModifierMapping({3,3,99});
    assert(invalid.target(0)==3 && invalid.target(1)==0 && invalid.target(2)==0);
    invalid=savedModifierMapping({"bad",-1,2});
    assert(invalid.target(0)==0 && invalid.target(1)==0 && invalid.target(2)==2);
    assert(savedModifierMapping({1,2}).target(1)==3);
    QTemporaryDir storage;
    assert(storage.isValid());
    const QString settingsPath=storage.filePath("test.ini");
    {
        QSettings settings(settingsPath,QSettings::IniFormat);
        mapping.assign(0,2); mapping.assign(1,1);
        settings.setValue("native/modifierMapping",modifierMappingValues(mapping));
        settings.sync(); assert(settings.status()==QSettings::NoError);
    }
    QSettings settings(settingsPath,QSettings::IniFormat);
    auto restored=savedModifierMapping(settings.value("native/modifierMapping").toList());
    assert(restored.packed()==mapping.packed());
    assert(ModifierMapping::fromPacked(mapping.packed()).packed()==mapping.packed());
    assert(validToken("abc.def-123"));
    assert(!validToken("abc\nsecret"));
    assert(!validToken("abc\n") && !validToken("abc\r\n") && !validToken("abc\r"));
    assert(validToken(QString(16384, 'a')));
    assert(!validToken(QString(16384, 'a') + '\n'));
    assert(!validToken(QString(16385, 'a')));
    assert(accountLimit({{"user", QJsonObject{{"currentPlan", "pro"}, {"resolution", "1080"}}}}) == 1080);
    assert(accountLimit({{"user", QJsonObject{{"currentPlan", "pro"}, {"resolution", 1080}}}}) == 0);
    assert(!connected({{"connected", "true"}}));
    assert(!connected({{"connected", 1}}));
    assert(connected({{"connected", true}}));
    assert(pinAccepted({{"message", "success"}, {"data", QJsonObject{{"status", true}}}}));
    assert(!pinAccepted({{"message", "success"}, {"data", QJsonObject{{"status", "true"}}}}));
    assert(vmAddress({{"vmip", "192.0.2.10"}}) == "192.0.2.10");
    assert(vmAddress({{"vmip", "127.0.0.1"}}).isEmpty());
    assert(vmAddress({{"vmip", "0.0.0.0"}}).isEmpty());
    assert(vmAddress({{"vmip", "224.0.0.1"}}).isEmpty());
    assert(vmAddress({{"vmip", "https://example.com"}}).isEmpty());
    StreamSettings s;
    assert(s.isValid());
    assert(s.providerSpec() == "720 6000 60 10");
    const StreamSettings presets[] = {
        {1280, 720, 60, 6000}, {1920, 1080, 60, 6000},
        {2560, 1440, 60, 6000}, {3840, 2160, 60, 6000}
    };
    const int providerPresets[] = {720, 1080, 1440, 2160};
    const char* providerSpecs[] = {
        "720 6000 60 10", "1080 6000 60 10", "1440 6000 60 10", "2160 6000 60 10"
    };
    for (int i = 0; i < 4; ++i) {
        assert(presets[i].isValid());
        assert(presets[i].preset() == providerPresets[i]);
        assert(presets[i].providerSpec() == QLatin1String(providerSpecs[i]));
    }
    s.width = 1440; s.height = 900;
    assert(!s.isValid() && s.preset() == 0);
    s.width = 1440; s.height = 901;
    assert(!s.isValid());
    for (int old = 0; old < 5; ++old) {
        const int migrated[] = {0, 1, 1, 2, 3};
        assert(savedResolutionIndex(old, 1) == migrated[old]);
    }
    for (int current = 0; current < 4; ++current) assert(savedResolutionIndex(current, 2) == current);
    assert(savedResolutionIndex(99, 2) == 1);
    assert(savedResolutionIndex(-1, 1) == 1);
    assert(savedResolutionIndex(settings.value("native/resolutionIndex", DefaultResolutionIndex).toInt(), 2) == 1);
    settings.setValue("native/resolutionIndex", 0);
    assert(savedResolutionIndex(settings.value("native/resolutionIndex", DefaultResolutionIndex).toInt(), 2) == 0);
    for (int bitrate : BitrateChoices) assert(savedBitrate(bitrate) == bitrate);
    assert(savedBitrate(45000) == 45000 && savedBitrate(50000) == 50000);
    assert(savedBitrate(1999) == 6000 && savedBitrate(50001) == 6000);
    assert(savedCodecIndex(-1) == 0 && savedCodecIndex(3) == 0);
    for (int codec = 0; codec < 3; ++codec) assert(savedCodecIndex(codec) == codec);
    assert(providerMinutes(0) == 0 && providerMinutes(123) == 123);
    assert(providerMinutes("123") == -1 && providerMinutes(true) == -1);
    assert(providerMinutes(-1) == -1 && providerMinutes(1.5) == -1);
    assert(providerMinutes(2147483648.0) == -1);
    assert(providerMinutes(QJsonValue()) == -1);
    assert(accountRemainingMinutes({{"user", QJsonObject{{"totalTimeMonth", 1000}, {"timeUsedMonth", 250}}}}) == 750);
    assert(accountRemainingMinutes({{"user", QJsonObject{{"totalTimeMonth", 1000}, {"timeUsedMonth", 1200}}}}) == 0);
    assert(accountRemainingMinutes({{"user", QJsonObject{{"totalTimeMonth", 0}, {"timeUsedMonth", 0}}}}) == -1);
    assert(accountRemainingMinutes({{"user", QJsonObject{{"totalTimeMonth", "1000"}, {"timeUsedMonth", 250}}}}) == -1);
    assert(accountRemainingMinutes({}) == -1);
    s = {}; s.fps = 61;
    assert(!s.isValid());
    for (int bitrate : {45000, 50000}) {
        s = {}; s.bitrate = bitrate;
        assert(s.isValid());
        assert(s.providerSpec() == QString("720 %1 60 10").arg(bitrate));
    }
    s = {}; s.bitrate = 1999;
    assert(!s.isValid());
    s = {}; s.bitrate = 50001;
    assert(!s.isValid());
    auto frame = parseFrame("42[\"new_connection\",{\"clients\":2}]");
    assert(frame.kind == Frame::Event && frame.name == "new_connection");
    assert(frame.value.toObject().value("clients").toInt() == 2);
    frame = parseFrame("427[\"status\",\"done\"]");
    assert(frame.kind == Frame::Event && frame.ackId == "7" && frame.value == "done");
    assert(parseFrame("2").kind == Frame::Ping);
    assert(parseFrame("40{\"sid\":\"hidden\"}").kind == Frame::NamespaceReady);
    assert(parseFrame("42[false,\"bad\"]").kind == Frame::Invalid);
    assert(parseFrame(QString(65537, 'x')).kind == Frame::Invalid);
    auto hello = parseFrame("0{\"pingInterval\":25000,\"pingTimeout\":20000}");
    assert(hello.kind == Frame::Hello && hello.heartbeatMs == 45000);
    assert(parseFrame("0{\"pingInterval\":-1,\"pingTimeout\":20000}").kind == Frame::Invalid);
    assert(parseFrame("0{\"pingInterval\":\"25000\",\"pingTimeout\":20000}").kind == Frame::Invalid);
    assert(eventPacket("closestream") == "42[\"closestream\"]");
    assert(eventPacket("control", "video") == "42[\"control\",\"video\"]");
}
