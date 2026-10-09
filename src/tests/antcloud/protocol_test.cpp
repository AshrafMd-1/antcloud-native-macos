#include "antcloud/protocol.h"
#include <QCoreApplication>
#include <cassert>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    using namespace AntCloud;
    assert(validToken("abc.def-123"));
    assert(!validToken("abc\nsecret"));
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
    s.width = 1440; s.height = 900;
    assert(!s.isValid() && s.preset() == 0);
    s.width = 1440; s.height = 901;
    assert(!s.isValid());
    for (int old = 0; old < 5; ++old) {
        const int migrated[] = {0, 1, 1, 2, 3};
        assert(savedResolutionIndex(old, 1) == migrated[old]);
    }
    for (int current = 0; current < 4; ++current) assert(savedResolutionIndex(current, 2) == current);
    assert(savedResolutionIndex(99, 2) == 0);
    assert(savedResolutionIndex(-1, 1) == 0);
    assert(savedBitrate(2000) == 6000 && savedBitrate(12000) == 12000);
    s = {}; s.fps = 61;
    assert(!s.isValid());
    s = {}; s.bitrate = 40001;
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
