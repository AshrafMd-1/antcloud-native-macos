// AntCloud Native additions, GPL-3.0-or-later, 2026.
#pragma once
#include "SDL_compat.h"
#include <SDL_ttf.h>
#include "path.h"
#include "antcloud/streambridge.h"

// This window shares the stream's SDL event loop. Its input never reaches Windows.
class AntCloudControls {
public:
    enum Action { None, Resume, Stop, Shutdown };
    ~AntCloudControls() { close(); }
    bool enabled() const { return AntCloud::StreamBridge::enabled(); }
    void hide() { if (m_Window) SDL_HideWindow(m_Window); }
    void close() {
        if (m_Font) TTF_CloseFont(m_Font);
        if (m_Renderer) SDL_DestroyRenderer(m_Renderer);
        if (m_Window) SDL_DestroyWindow(m_Window);
        m_Font = nullptr; m_Renderer = nullptr; m_Window = nullptr;
    }
    bool show(SDL_Window* stream) {
        if (!enabled()) return false;
        m_Pressed = None;
        m_Focused = Resume;
        if (!m_Window) {
            m_FontData = Path::readDataFile("ModeSeven.ttf");
            m_Font = TTF_OpenFontRW(SDL_RWFromConstMem(m_FontData.constData(), m_FontData.size()), 1, 20);
            m_Window = SDL_CreateWindow("AntCloud Native — stream controls", SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED, 660, 416, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_ALWAYS_ON_TOP);
            if (m_Window) m_Renderer = SDL_CreateRenderer(m_Window, -1, SDL_RENDERER_SOFTWARE);
            if (!m_Font || !m_Window || !m_Renderer) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Local controls could not open: %s", SDL_GetError());
                close(); return false;
            }
            SDL_RenderSetLogicalSize(m_Renderer, 660, 416);
        }
        int x, y, w, h;
        SDL_GetWindowPosition(stream, &x, &y); SDL_GetWindowSize(stream, &w, &h);
        SDL_Rect bounds;
        int panelX = x + (w - 660) / 2, panelY = y + 64;
        if (SDL_GetDisplayUsableBounds(SDL_GetWindowDisplayIndex(stream), &bounds) == 0) {
            panelX = SDL_max(bounds.x, SDL_min(panelX, bounds.x + bounds.w - 660));
            panelY = SDL_max(bounds.y, SDL_min(panelY, bounds.y + bounds.h - 416));
        }
        SDL_SetWindowPosition(m_Window, panelX, panelY);
        SDL_ShowWindow(m_Window); SDL_RaiseWindow(m_Window); render();
        return true;
    }
    bool handles(const SDL_Event& event) const {
        if (!m_Window) return false;
        Uint32 id = 0;
        switch (event.type) {
        case SDL_WINDOWEVENT: id = event.window.windowID; break;
        case SDL_KEYDOWN: case SDL_KEYUP: id = event.key.windowID; break;
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: id = event.button.windowID; break;
        case SDL_MOUSEMOTION: id = event.motion.windowID; break;
        case SDL_MOUSEWHEEL: id = event.wheel.windowID; break;
        case SDL_TEXTINPUT: id = event.text.windowID; break;
        case SDL_TEXTEDITING: id = event.edit.windowID; break;
        default: return false;
        }
        return id == SDL_GetWindowID(m_Window);
    }
    Action handle(const SDL_Event& event) {
        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) return Resume;
            if (event.window.event == SDL_WINDOWEVENT_EXPOSED) render();
        }
        if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            switch (event.key.keysym.sym) {
            case SDLK_ESCAPE: return Resume;
            case SDLK_RETURN: case SDLK_SPACE: return m_Focused;
            case SDLK_TAB:
                moveFocus((event.key.keysym.mod & KMOD_SHIFT) ? -1 : 1);
                return None;
            case SDLK_LEFT: moveFocus(-1); return None;
            case SDLK_RIGHT: moveFocus(1); return None;
            default: break;
            }
        }
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            m_Pressed = buttonAt(event.button.x, event.button.y);
            if (m_Pressed != None) { m_Focused = m_Pressed; render(); }
        }
        if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
            Action action = buttonAt(event.button.x, event.button.y);
            bool clicked = action != None && action == m_Pressed;
            m_Pressed = None;
            if (clicked) return action;
        }
        return None;
    }
    bool confirmShutdown() {
        const SDL_MessageBoxButtonData buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"},
            {0, 1, "Shutdown VM"}
        };
        const SDL_MessageBoxData dialog = {SDL_MESSAGEBOX_WARNING, m_Window, "Shutdown AntCloud VM?",
            "Save your work first. This requests the same shutdown control as AntCloud's browser.\n\n"
            "This is different from Stop stream, which sends no shutdown request.", 2, buttons, nullptr};
        int selected = 0;
        return SDL_ShowMessageBox(&dialog, &selected) == 0 && selected == 1;
    }
    bool requestShutdown() {
        return AntCloud::StreamBridge::shutdown();
    }
private:
    void moveFocus(int direction) {
        m_Focused = static_cast<Action>(1 + (static_cast<int>(m_Focused) - 1 + direction + 3) % 3);
        render();
    }
    static Action buttonAt(int x, int y) {
        if (y < 50 || y >= 96) return None;
        if (x >= 16 && x < 208) return Resume;
        if (x >= 224 && x < 432) return Stop;
        if (x >= 448 && x < 644) return Shutdown;
        return None;
    }
    void text(const char* value, int x, int y, SDL_Color color = {245, 245, 245, 255}) {
        SDL_Surface* surface = TTF_RenderUTF8_Blended(m_Font, value, color);
        if (!surface) return;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(m_Renderer, surface);
        if (texture) {
            SDL_Rect target = {x, y, surface->w, surface->h};
            SDL_RenderCopy(m_Renderer, texture, nullptr, &target); SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    }
    void render() {
        SDL_SetRenderDrawColor(m_Renderer, 24, 28, 36, 255); SDL_RenderClear(m_Renderer);
        text("Input released - choose a local action", 16, 14);
        const SDL_Rect boxes[] = {{16, 50, 192, 46}, {224, 50, 208, 46}, {448, 50, 196, 46}};
        for (int i = 0; i < 3; ++i) {
            SDL_SetRenderDrawColor(m_Renderer, i == 2 ? 135 : 48, i == 2 ? 40 : 59, i == 2 ? 46 : 74, 255);
            SDL_RenderFillRect(m_Renderer, &boxes[i]);
            if (m_Focused == i + 1) {
                SDL_SetRenderDrawColor(m_Renderer, 130, 177, 255, 255);
                SDL_RenderDrawRect(m_Renderer, &boxes[i]);
            }
        }
        text("Resume", 62, 64); text("Stop stream", 254, 64); text("Shutdown VM", 472, 64);
        SDL_SetRenderDrawColor(m_Renderer, 77, 85, 100, 255);
        SDL_RenderDrawLine(m_Renderer, 16, 118, 644, 118);
        text("Ctrl + Cmd + Shift shortcuts", 16, 136);
        const char* shortcuts[][2] = {
            {"P", "Stream controls"}, {"Q", "Stop stream"}, {"Z", "Release input"},
            {"X", "Fullscreen"}, {"S", "Stream stats"}, {"M", "Mouse mode"},
            {"B", "Microphone"}, {"V", "Type clipboard"}, {"C", "Show cursor"},
            {"D", "Minimize"}, {"L", "Pointer lock"}, {"K", "Keyboard capture"},
            {"E", "Stop and exit"}, {"H", "Controls hint"}
        };
        for (size_t i = 0; i < SDL_arraysize(shortcuts); ++i) {
            int x = 16 + (i / 7) * 324, y = 176 + (i % 7) * 32;
            text(shortcuts[i][0], x, y, {130, 177, 255, 255});
            text(shortcuts[i][1], x + 34, y);
        }
        SDL_RenderPresent(m_Renderer);
    }
    SDL_Window* m_Window = nullptr;
    SDL_Renderer* m_Renderer = nullptr;
    TTF_Font* m_Font = nullptr;
    QByteArray m_FontData;
    Action m_Pressed = None;
    Action m_Focused = Resume;
};
