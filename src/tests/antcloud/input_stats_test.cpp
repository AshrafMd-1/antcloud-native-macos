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
    assert(remote.keysym.scancode == SDL_SCANCODE_LCTRL);
    assert(remote.keysym.mod == (KMOD_LCTRL | KMOD_RALT | KMOD_CAPS));
    assert(remote.state == event.state && remote.type == event.type);
    assert(event.keysym.scancode == SDL_SCANCODE_LGUI); // Never mutate physical state.

    const SDL_Scancode physical[] = {SDL_SCANCODE_RGUI, SDL_SCANCODE_LALT, SDL_SCANCODE_RALT, SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL, SDL_SCANCODE_F1, SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_DELETE};
    const SDL_Scancode expected[] = {SDL_SCANCODE_RCTRL, SDL_SCANCODE_LGUI, SDL_SCANCODE_RGUI, SDL_SCANCODE_LALT, SDL_SCANCODE_RALT, SDL_SCANCODE_F1, SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_DELETE};
    for (int i = 0; i < 8; ++i) {
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
    assert(AntCloudInput::mapMacModifiers(event).keysym.mod == KMOD_LALT);
    event.keysym.mod = KMOD_RALT;
    assert(AntCloudInput::mapMacModifiers(event).keysym.mod == KMOD_RGUI);

    event.keysym.mod = KMOD_LGUI | KMOD_RALT | KMOD_LSHIFT;
    assert(AntCloudInput::mapMacModifiers(event).keysym.mod == (KMOD_LCTRL | KMOD_RGUI | KMOD_LSHIFT));

    const SDL_Scancode sources[3][2] = {{SDL_SCANCODE_LGUI, SDL_SCANCODE_RGUI}, {SDL_SCANCODE_LALT, SDL_SCANCODE_RALT}, {SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL}};
    const SDL_Scancode targets[4][2] = {{SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN}, {SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL}, {SDL_SCANCODE_LALT, SDL_SCANCODE_RALT}, {SDL_SCANCODE_LGUI, SDL_SCANCODE_RGUI}};
    const Uint16 sourceFlags[3][2] = {{KMOD_LGUI, KMOD_RGUI}, {KMOD_LALT, KMOD_RALT}, {KMOD_LCTRL, KMOD_RCTRL}};
    const Uint16 targetFlags[4][2] = {{0,0}, {KMOD_LCTRL, KMOD_RCTRL}, {KMOD_LALT, KMOD_RALT}, {KMOD_LGUI, KMOD_RGUI}};
    for (int row=0; row<3; ++row) for (int target=0; target<4; ++target) {
        AntCloud::ModifierMapping mapping;
        mapping.assign(row,target);
        for (int other=0; other<3; ++other)
            if (other!=row && target!=0) assert(mapping.target(other)!=target);
        for (int side=0; side<2; ++side) for (int pressed=0; pressed<2; ++pressed) {
            event.type=pressed ? SDL_KEYDOWN : SDL_KEYUP;
            event.state=pressed ? SDL_PRESSED : SDL_RELEASED;
            event.keysym.scancode=sources[row][side];
            event.keysym.mod=(pressed ? sourceFlags[row][side] : 0) | KMOD_SHIFT | KMOD_CAPS;
            remote=AntCloudInput::mapMacModifiers(event,mapping);
            assert(remote.keysym.scancode==targets[target][side]);
            assert(remote.keysym.mod==((pressed ? targetFlags[target][side] : 0) | KMOD_SHIFT | KMOD_CAPS));
            assert(remote.type==event.type && remote.state==event.state);
            assert(event.keysym.scancode==sources[row][side]);
        }
        for (int pressed=0; pressed<2; ++pressed) {
            event.type=pressed ? SDL_KEYDOWN : SDL_KEYUP;
            event.state=pressed ? SDL_PRESSED : SDL_RELEASED;
            event.keysym.scancode=SDL_SCANCODE_A;
            event.keysym.mod=pressed ? sourceFlags[row][0] : 0;
            remote=AntCloudInput::mapMacModifiers(event,mapping);
            assert(remote.keysym.scancode==SDL_SCANCODE_A);
            assert(remote.keysym.mod==(pressed ? targetFlags[target][0] : 0));
        }
    }
    AntCloud::ModifierMapping disabled(std::array<int,3>{{0,0,0}});
    event.keysym.scancode=SDL_SCANCODE_P;
    event.keysym.mod=KMOD_LGUI | KMOD_RCTRL | KMOD_SHIFT;
    assert(AntCloudInput::isMacLocalChord(event.keysym.mod));

    event.keysym.scancode=SDL_SCANCODE_B;
    remote=AntCloudInput::mapMacModifiers(event,disabled);
    assert(remote.keysym.scancode==SDL_SCANCODE_B && remote.keysym.mod==KMOD_SHIFT);
    assert(AntCloudInput::isMacLocalChord(event.keysym.mod)); // Physical microphone chord survives None mappings.
    assert(!AntCloudInput::isMacLocalChord(KMOD_LGUI | KMOD_SHIFT));
    event.keysym.scancode=SDL_SCANCODE_P;
    remote=AntCloudInput::mapMacModifiers(event,disabled);
    assert(remote.keysym.scancode==SDL_SCANCODE_P && remote.keysym.mod==KMOD_SHIFT);
    assert(AntCloudInput::isMacLocalChord(event.keysym.mod));

    char text[200];
    AntCloudStats::format({32, 120, 3, 58.5, 5.8}, text, sizeof(text));
    assert(std::string(text) == "RTT 32 ms | Loss 2.5% | 58.5 FPS | 5.8 Mbps | Time left unavailable");
    AntCloudStats::format({0, 0, 0, 0, 0}, text, sizeof(text));
    assert(std::string(text).find("RTT N/A | Loss N/A") == 0);
    assert(std::string(text).find("N/A FPS | N/A") != std::string::npos);
    AntCloudStats::format({32, 120, 0, 60, 6, 750}, text, sizeof(text));
    assert(std::string(text).find(" | Time left 12h 30m") != std::string::npos);
    AntCloudStats::format({32, 120, 0, 60, 6, 0}, text, sizeof(text));
    assert(std::string(text).find(" | Time left 0h 0m") != std::string::npos);
    assert(std::string(text).find("\n") == std::string::npos);
    assert(std::string(text).find("1920") == std::string::npos);
    char tiny[5] = {};
    AntCloudStats::format({32, 120, 0, 60, 6}, tiny, sizeof(tiny));
    assert(tiny[4] == '\0');
    AntCloudStats::format({0, 1, 0, std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity()}, text, sizeof(text));
    assert(std::string(text).find("N/A FPS | N/A") != std::string::npos);
}
