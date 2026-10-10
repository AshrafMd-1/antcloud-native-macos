#include "tokenstore.h"
#include "protocol.h"
#ifdef ANTCLOUD_TESTING
#include <QMutex>
#include <QMutexLocker>
namespace {
QMutex storageMutex;
QByteArray testStoredLogin;
}
#endif
#if defined(Q_OS_MACOS) && !defined(ANTCLOUD_TESTING)
#include <Security/Security.h>
#include <CoreFoundation/CoreFoundation.h>

namespace {
CFMutableDictionaryRef query() {
    auto value = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(value, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(value, kSecAttrService, CFSTR("day.ashraf.antcloud.native.login"));
    CFDictionarySetValue(value, kSecAttrAccount, CFSTR("access-token"));
    CFDictionarySetValue(value, kSecAttrSynchronizable, kCFBooleanFalse);
    return value;
}
}
#endif

AntCloud::TokenStore::Credentials AntCloud::TokenStore::decode(const QByteArray& bytes, QString& error) {
    error.clear();
    if (bytes.isEmpty()) return {};
    const QString legacy = QString::fromUtf8(bytes);
    if (validToken(legacy)) return {legacy, {}};
    if (bytes.size() <= 65536) {
        const auto value = QJsonDocument::fromJson(bytes).object();
        const QString access = value.value("accessToken").toString(), refresh = value.value("refreshToken").toString();
        if (value.value("version") == QJsonValue(1) && value.value("accessToken").isString()
            && value.value("refreshToken").isString() && validToken(access) && (refresh.isEmpty() || validToken(refresh)))
            return {access, refresh};
    }
    error = "The saved login has an invalid format. Sign in again to replace it.";
    return {};
}

AntCloud::TokenStore::Credentials AntCloud::TokenStore::load(QString& error) {
    error.clear();
#if defined(ANTCLOUD_TESTING)
    QMutexLocker lock(&storageMutex);
    return decode(testStoredLogin, error);
#elif defined(Q_OS_MACOS)
    auto request = query();
    CFDictionarySetValue(request, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(request, kSecMatchLimit, kSecMatchLimitOne);
    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching(request, &result);
    CFRelease(request);
    if (status == errSecItemNotFound) return {};
    if (status != errSecSuccess) {
        error = QStringLiteral("Keychain could not read the saved login. You can sign in for this run.");
        return {};
    }
    QByteArray bytes;
    if (result && CFGetTypeID(result) == CFDataGetTypeID()) {
        auto data = static_cast<CFDataRef>(result);
        if (CFDataGetLength(data) <= 65536)
            bytes = QByteArray(reinterpret_cast<const char*>(CFDataGetBytePtr(data)), CFDataGetLength(data));
    }
    if (result) CFRelease(result);
    if (bytes.isEmpty()) { error = "The saved login has an invalid format. Sign in again to replace it."; return {}; }
    return decode(bytes, error);
#else
    return {};
#endif
}

bool AntCloud::TokenStore::save(const Credentials& credentials, QString& error) {
    error.clear();
    if (!validToken(credentials.accessToken) || (!credentials.refreshToken.isEmpty() && !validToken(credentials.refreshToken))) {
        error = "Login token has an invalid format."; return false;
    }
    auto bytes = QJsonDocument(QJsonObject{{"version", 1}, {"accessToken", credentials.accessToken},
        {"refreshToken", credentials.refreshToken}}).toJson(QJsonDocument::Compact);
#if defined(ANTCLOUD_TESTING)
    QMutexLocker lock(&storageMutex); testStoredLogin = bytes; return true;
#elif defined(Q_OS_MACOS)
    auto data = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(bytes.constData()), bytes.size());
    auto request = query();
    auto update = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(update, kSecValueData, data);
    OSStatus status = SecItemUpdate(request, update);
    if (status == errSecItemNotFound) {
        CFDictionarySetValue(request, kSecValueData, data);
        CFDictionarySetValue(request, kSecAttrAccessible, kSecAttrAccessibleWhenUnlockedThisDeviceOnly);
        status = SecItemAdd(request, nullptr);
    }
    CFRelease(update); CFRelease(request); CFRelease(data);
    bytes.fill('\0');
    if (status == errSecSuccess) return true;
    error = "Keychain could not save the login. This run can continue; you will need to sign in next time.";
#else
    error = "Remembered login is available in the macOS build only.";
#endif
    return false;
}

bool AntCloud::TokenStore::clear(QString& error) {
    error.clear();
#if defined(ANTCLOUD_TESTING)
    QMutexLocker lock(&storageMutex); testStoredLogin.fill('\0'); testStoredLogin.clear(); return true;
#elif defined(Q_OS_MACOS)
    auto request = query();
    OSStatus status = SecItemDelete(request);
    CFRelease(request);
    if (status == errSecSuccess || status == errSecItemNotFound) return true;
    error = "Keychain could not remove the saved login. Remove the AntCloud login item in Passwords / Keychain Access.";
    return false;
#else
    return true;
#endif
}
