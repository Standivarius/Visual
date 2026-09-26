#pragma once

#include "core/poi_evidence.h"

#include <windows.h>

namespace visual::ui {

class ContextOverlay {
public:
    ContextOverlay() = default;
    ContextOverlay(const ContextOverlay&) = delete;
    ContextOverlay& operator=(const ContextOverlay&) = delete;
    ~ContextOverlay();

    bool create(HINSTANCE instance, const RECT& source_monitor_rect);
    void update(const visual::core::ScreenRect& viewport, double zoom, bool enabled, bool shade);
    void hide() noexcept;
    void destroy() noexcept;

    [[nodiscard]] HWND hwnd() const noexcept { return hwnd_; }
    [[nodiscard]] bool excluded_from_capture() const noexcept { return excluded_from_capture_; }

private:
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
    bool ensure_surface(int width, int height);
    void draw_surface(int width, int height, bool shade);
    void release_surface() noexcept;

    HWND hwnd_{};
    RECT source_monitor_rect_{};
    bool excluded_from_capture_{};
    bool visible_{};
    bool last_shade_{};
    RECT last_rect_{};
    HDC memory_dc_{};
    HBITMAP bitmap_{};
    HGDIOBJ old_bitmap_{};
    void* bitmap_bits_{};
    int bitmap_width_{};
    int bitmap_height_{};
};

} // namespace visual::ui
