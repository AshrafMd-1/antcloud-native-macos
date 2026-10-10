// AntCloud Native additions, GPL-3.0-or-later, 2026.
#pragma once
#include "SDL_compat.h"
#include "antcloud/modifiermapping.h"

namespace AntCloudInput {
inline bool isMacLocalChord(Uint16 modifiers) {
    return (modifiers & KMOD_CTRL) && (modifiers & KMOD_GUI) && (modifiers & KMOD_SHIFT);
}

// Translate a copy only after checking local controls in physical keys.
inline SDL_KeyboardEvent mapMacModifiers(SDL_KeyboardEvent event,
                                        const AntCloud::ModifierMapping& mapping = {})
{
    const SDL_Scancode physicalKeys[3][2] = {
        {SDL_SCANCODE_LGUI, SDL_SCANCODE_RGUI},
        {SDL_SCANCODE_LALT, SDL_SCANCODE_RALT},
        {SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL}
    };
    const SDL_Scancode remoteKeys[4][2] = {
        {SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_UNKNOWN},
        {SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL},
        {SDL_SCANCODE_LALT, SDL_SCANCODE_RALT},
        {SDL_SCANCODE_LGUI, SDL_SCANCODE_RGUI}
    };
    const Uint16 physicalFlags[3][2] = {{KMOD_LGUI, KMOD_RGUI}, {KMOD_LALT, KMOD_RALT}, {KMOD_LCTRL, KMOD_RCTRL}};
    const Uint16 remoteFlags[4][2] = {{0, 0}, {KMOD_LCTRL, KMOD_RCTRL}, {KMOD_LALT, KMOD_RALT}, {KMOD_LGUI, KMOD_RGUI}};
    const auto scancode = event.keysym.scancode;
    const auto modifiers = event.keysym.mod;
    event.keysym.mod &= ~(KMOD_CTRL | KMOD_ALT | KMOD_GUI);
    for (int row = 0; row < 3; ++row) {
        const int target = mapping.target(row);
        for (int side = 0; side < 2; ++side) {
            if (scancode == physicalKeys[row][side]) event.keysym.scancode = remoteKeys[target][side];
            if (modifiers & physicalFlags[row][side]) event.keysym.mod |= remoteFlags[target][side];
        }
    }
    return event;
}
}
