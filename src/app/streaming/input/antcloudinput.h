// AntCloud Native additions, GPL-3.0-or-later, 2026.
#pragma once
#include "SDL_compat.h"

namespace AntCloudInput {
// Only remote SDL events are translated. Physical modifiers still identify the
// client's escape/toggle combinations before this function is called.
inline SDL_KeyboardEvent mapMacModifiers(SDL_KeyboardEvent event)
{
    switch (event.keysym.scancode) {
    case SDL_SCANCODE_LGUI: event.keysym.scancode = SDL_SCANCODE_LALT; break;
    case SDL_SCANCODE_RGUI: event.keysym.scancode = SDL_SCANCODE_RALT; break;
    case SDL_SCANCODE_LALT: event.keysym.scancode = SDL_SCANCODE_LGUI; break;
    case SDL_SCANCODE_RALT: event.keysym.scancode = SDL_SCANCODE_RGUI; break;
    default: break;
    }
    const auto physical = event.keysym.mod;
    event.keysym.mod &= ~(KMOD_ALT | KMOD_GUI);
    if (physical & KMOD_LGUI) event.keysym.mod |= KMOD_LALT;
    if (physical & KMOD_RGUI) event.keysym.mod |= KMOD_RALT;
    if (physical & KMOD_LALT) event.keysym.mod |= KMOD_LGUI;
    if (physical & KMOD_RALT) event.keysym.mod |= KMOD_RGUI;
    return event;
}
}
