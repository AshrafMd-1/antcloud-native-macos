#include "tokenstore.h"
#include "protocol.h"
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

QString AntCloud::TokenStore::load(QString& error) {
    error.clear();
#if defined(Q_OS_MACOS) && !defined(ANTCLOUD_TESTING)
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
    QString token;
    if (result && CFGetTypeID(result) == CFDataGetTypeID()) {
        auto data = static_cast<CFDataRef>(result);
        if (CFDataGetLength(data) <= 16384)
            token = QString::fromUtf8(reinterpret_cast<const char*>(CFDataGetBytePtr(data)), CFDataGetLength(data));
    }
    if (result) CFRelease(result);
    if (!validToken(token)) {
        error = QStringLiteral("The saved login has an invalid format. Sign in again to replace it.");
        return {};
    }
    return token;
#else
    return {};
#endif
}

bool AntCloud::TokenStore::save(const QString& token, QString& error) {
    error.clear();
    if (!validToken(token)) { error = "Login token has an invalid format."; return false; }
#if defined(Q_OS_MACOS) && !defined(ANTCLOUD_TESTING)
    const auto bytes = token.toUtf8();
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
    if (status == errSecSuccess) return true;
    error = "Keychain could not save the login. This run can continue; you will need to sign in next time.";
#else
    error = "Remembered login is available in the macOS build only.";
#endif
    return false;
}

bool AntCloud::TokenStore::clear(QString& error) {
    error.clear();
#if defined(Q_OS_MACOS) && !defined(ANTCLOUD_TESTING)
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
