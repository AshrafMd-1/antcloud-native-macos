// AntCloud Native additions, GPL-3.0-or-later, 2026.
#pragma once
#include <QString>

namespace AntCloud {
class TokenStore {
public:
    static QString load(QString& error);
    static bool save(const QString& token, QString& error);
    static bool clear(QString& error);
};
}
