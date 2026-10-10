// AntCloud Native additions, GPL-3.0-or-later, 2026.
#pragma once
#include <QString>
#include <QByteArray>

namespace AntCloud {
class TokenStore {
public:
    struct Credentials { QString accessToken, refreshToken; };
    static Credentials load(QString& error);
    static bool save(const Credentials& credentials, QString& error);
    static bool clear(QString& error);
private:
    static Credentials decode(const QByteArray& bytes, QString& error);
#ifdef ANTCLOUD_TESTING
    friend class ProviderWorkerTest;
#endif
};
}
