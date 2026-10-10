#include "microphonecapture.h"
namespace AntCloud::MicrophoneAudio {
void requestPermission(std::function<void(bool)> completion) { completion(false); }
std::unique_ptr<Capture> makeCapture() { return {}; }
}
