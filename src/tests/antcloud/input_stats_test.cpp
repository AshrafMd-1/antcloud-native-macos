#include "antcloudinput.h"
#include "antcloudstats.h"
#include <cassert>
#include <string>
#include <limits>

int main()
{
    SDL_KeyboardEvent event{};
    event.type = SDL_KEYDOWN;
    event.state = SDL_PRESSED;
    event.keysym.scancode = SDL_SCANCODE_LGUI;
    event.keysym.mod = KMOD_LGUI | KMOD_RCTRL | KMOD_CAPS;
    auto remote = AntCloudInput::mapMacModifiers(event);
    assert(remote.keysym.scancode == SDL_SCANCODE_LALT);
    assert(remote.keysym.mod == (KMOD_LALT | KMOD_RCTRL | KMOD_CAPS));
    assert(remote.state == event.state && remote.type == event.type);
    assert(event.keysym.scancode == SDL_SCANCODE_LGUI); // Never mutate physical state.

    const SDL_Scancode physical[] = {SDL_SCANCODE_RGUI, SDL_SCANCODE_LALT, SDL_SCANCODE_RALT, SDL_SCANCODE_LCTRL, SDL_SCANCODE_F1};
    const SDL_Scancode expected[] = {SDL_SCANCODE_RALT, SDL_SCANCODE_LGUI, SDL_SCANCODE_RGUI, SDL_SCANCODE_LCTRL, SDL_SCANCODE_F1};
    for (int i = 0; i < 5; ++i) {
        event.type = SDL_KEYUP;
        event.state = SDL_RELEASED;
        event.keysym.scancode = physical[i];
        remote = AntCloudInput::mapMacModifiers(event);
        assert(remote.keysym.scancode == expected[i]);
        assert(remote.state == SDL_RELEASED);
    }
    // Both sides together and all unaffected flags survive the permutation.
    event.keysym.mod = KMOD_ALT | KMOD_GUI | KMOD_CTRL | KMOD_SHIFT | KMOD_NUM;
    assert(AntCloudInput::mapMacModifiers(event).keysym.mod == event.keysym.mod);
    event.keysym.scancode = SDL_SCANCODE_A;
    event.keysym.mod = KMOD_LCTRL;
    assert(AntCloudInput::mapMacModifiers(event).keysym.scancode == SDL_SCANCODE_A);
    assert(AntCloudInput::mapMacModifiers(event).keysym.mod == KMOD_LCTRL);
    event.keysym.mod = KMOD_RALT;
    assert(AntCloudInput::mapMacModifiers(event).keysym.mod == KMOD_RGUI);

    char text[200];
    AntCloudStats::format({32, 120, 3, 58.5, 5.8, 1440, 900}, text, sizeof(text));
    assert(std::string(text) == "RTT 32 ms | Loss 2.5% | 58.5 FPS | 5.8 Mbps | 1440x900");
    AntCloudStats::format({0, 0, 0, 0, 0, 1280, 720}, text, sizeof(text));
    assert(std::string(text).find("RTT N/A | Loss N/A") == 0);
    assert(std::string(text).find("N/A FPS | N/A") != std::string::npos);
    char tiny[5] = {};
    AntCloudStats::format({32, 120, 0, 60, 6, 1920, 1080}, tiny, sizeof(tiny));
    assert(tiny[4] == '\0');
    AntCloudStats::format({0, 1, 0, std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(), 1280, 720}, text, sizeof(text));
    assert(std::string(text).find("N/A FPS | N/A") != std::string::npos);
}
