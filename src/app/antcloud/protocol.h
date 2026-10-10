// AntCloud Native additions, GPL-3.0-or-later, 2026.
#pragma once
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <cmath>
#include <limits>
#include <QVariantList>
#include "modifiermapping.h"

namespace AntCloud {
constexpr int StartupTimeoutMs = 300000;
constexpr int DefaultResolutionIndex = 1;
inline ModifierMapping savedModifierMapping(const QVariantList& values) {
    if (values.size() != 3) return {};
    std::array<int, 3> targets{};
    for (int row = 0; row < 3; ++row) {
        bool valid;
        targets[row] = values[row].toInt(&valid);
        if (!valid) targets[row] = 0;
    }
    return ModifierMapping(targets);
}
inline QVariantList modifierMappingValues(const ModifierMapping& mapping) {
    return {mapping.target(0), mapping.target(1), mapping.target(2)};
}
inline int savedResolutionIndex(int index, int schema) {
    if (schema < 2) {
        if (index == 2) return 1; // Schema 1 index 2 was 1440×900; migrate it to 1080p.
        if (index == 3 || index == 4) return index - 1;
    }
    return index >= 0 && index <= 3 ? index : DefaultResolutionIndex;
}
constexpr int BitrateChoices[] = {2000, 3000, 4000, 6000, 8000, 10000, 12000, 15000, 20000, 25000, 30000, 35000, 40000, 45000, 50000};
inline int savedBitrate(int bitrate) {
    for (int choice : BitrateChoices) if (bitrate == choice) return bitrate;
    return 6000;
}
inline int savedCodecIndex(int index) { return index >= 0 && index <= 2 ? index : 0; }
inline int providerMinutes(const QJsonValue& value) {
    if (!value.isDouble()) return -1;
    const double minutes = value.toDouble();
    return std::isfinite(minutes) && minutes >= 0 && minutes <= std::numeric_limits<int>::max()
        && std::floor(minutes) == minutes ? int(minutes) : -1;
}
inline int accountRemainingMinutes(const QJsonObject& value) {
    const auto user = value.value("user").toObject();
    const int total = providerMinutes(user.value("totalTimeMonth"));
    const int used = providerMinutes(user.value("timeUsedMonth"));
    return total > 0 && used >= 0 ? qMax(0, total - used) : -1;
}
inline bool validToken(const QString& token) {
    static const QRegularExpression pattern(QRegularExpression::anchoredPattern("[A-Za-z0-9_.~+/=-]{1,16384}"));
    return pattern.match(token).hasMatch();
}
inline int accountLimit(const QJsonObject& response) {
    const auto user = response.value("user").toObject();
    if (!user.value("currentPlan").isString() || user.value("currentPlan").toString().isEmpty()
        || !user.value("resolution").isString()) return 0;
    const QString value = user.value("resolution").toString();
    return value == "720" || value == "1080" || value == "1440" || value == "2160" ? value.toInt() : 0;
}
inline bool connected(const QJsonObject& value) {
    return value.value("connected").isBool() && value.value("connected").toBool();
}
inline bool pinAccepted(const QJsonObject& value) {
    const auto result = value.value("data").toObject().value("status");
    return value.value("message").toString() == "success" && result.isBool() && result.toBool();
}
inline QString vmAddress(const QJsonObject& value) {
    if (!value.value("vmip").isString()) return {};
    QHostAddress address;
    if (!address.setAddress(value.value("vmip").toString().trimmed()) || address.isNull()
        || address == QHostAddress::AnyIPv4 || address == QHostAddress::AnyIPv6
        || address.isLoopback() || address.isMulticast()) return {};
    return address.toString();
}
struct StreamSettings {
    int width = 1280, height = 720, fps = 60, bitrate = 6000;
    int preset() const {
        if (width == 1280 && height == 720) return 720;
        if (width == 1920 && height == 1080) return 1080;
        if (width == 2560 && height == 1440) return 1440;
        if (width == 3840 && height == 2160) return 2160;
        return 0;
    }
    bool isValid() const {
        return preset() && (fps == 30 || fps == 60 || fps == 120) && bitrate >= 2000 && bitrate <= 50000;
    }
    QString providerSpec() const {
        return QString("%1 %2 %3 10").arg(preset()).arg(bitrate).arg(fps);
    }
};
struct Frame {
    enum Kind { Invalid, Hello, Ping, NamespaceReady, Event, Closed, NamespaceError, Ignored } kind = Invalid;
    QString name, ackId;
    QJsonValue value;
    int heartbeatMs = 0;
};
inline Frame parseFrame(const QString& packet) {
    Frame frame;
    if (packet.size() > 65536) return frame;
    if (packet == "2") { frame.kind = Frame::Ping; return frame; }
    if (packet == "1" || packet.startsWith("41")) { frame.kind = Frame::Closed; return frame; }
    if (packet.startsWith("44")) { frame.kind = Frame::NamespaceError; return frame; }
    if (packet == "40" || packet.startsWith("40{")) { frame.kind = Frame::NamespaceReady; return frame; }
    if (packet.startsWith('0')) {
        const auto data = QJsonDocument::fromJson(packet.mid(1).toUtf8()).object();
        const auto interval = data.value("pingInterval"), timeout = data.value("pingTimeout");
        if (!interval.isDouble() || !timeout.isDouble() || interval.toDouble() < 1000
            || interval.toDouble() > 120000 || timeout.toDouble() < 1000 || timeout.toDouble() > 120000) return frame;
        frame.kind = Frame::Hello;
        frame.heartbeatMs = interval.toInt() + timeout.toInt();
        return frame;
    }
    if (!packet.startsWith("42")) { frame.kind = Frame::Ignored; return frame; }
    int pos = 2;
    while (pos < packet.size() && packet[pos].isDigit()) ++pos;
    frame.ackId = packet.mid(2, pos - 2);
    const auto doc = QJsonDocument::fromJson(packet.mid(pos).toUtf8());
    if (!doc.isArray()) return frame;
    const auto values = doc.array();
    if (values.isEmpty() || !values[0].isString() || values[0].toString().size() > 80) return frame;
    frame.kind = Frame::Event; frame.name = values[0].toString();
    if (values.size() > 1) frame.value = values[1];
    return frame;
}
inline QString eventPacket(const QString& name, const QJsonValue& value = QJsonValue(QJsonValue::Undefined)) {
    QJsonArray values{name};
    if (!value.isUndefined()) values.append(value);
    return "42" + QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Compact));
}
}
