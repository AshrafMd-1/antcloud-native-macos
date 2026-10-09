// Test-only boundaries: no compatibility download, box-art deletion or public
// connectivity/STUN probes. Real NvHTTP/NvComputer/ComputerManager/Seeker run.
#include "settings/streamingpreferences.h"
#include "settings/compatfetcher.h"
#include "backend/boxartmanager.h"
#include <Limelight.h>

StreamingPreferences::StreamingPreferences(QQmlEngine* engine) : QObject(engine), m_QmlEngine(engine) {
    enableMdns = false; detectNetworkBlocking = false;
}
StreamingPreferences* StreamingPreferences::get(QQmlEngine*) { static StreamingPreferences value(nullptr); return &value; }
int StreamingPreferences::getDefaultBitrate(int,int,int,bool) { return 6000; }
void StreamingPreferences::save() {}
void StreamingPreferences::reload() {}
bool StreamingPreferences::retranslate() { return false; }
CompatFetcher::CompatFetcher(QObject* parent) : QObject(parent), m_Nam(nullptr) {}
void CompatFetcher::start() {}
void CompatFetcher::handleCompatInfoFetched(QNetworkReply*) {}
bool CompatFetcher::isGfeVersionSupported(QString) { return true; }
void BoxArtManager::deleteBoxArt(NvComputer*) {}
extern "C" {
const char* LiGetLaunchUrlQueryParameters() { return ""; }
unsigned int LiTestClientConnectivity(const char*, unsigned short, unsigned int) { return ML_TEST_RESULT_INCONCLUSIVE; }
int LiFindExternalAddressIP4(const char*, unsigned short, unsigned int*) { return -1; }
}
