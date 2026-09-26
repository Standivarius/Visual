#include "context_overlay.h"

#include "core/settings_model.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace visual::ui {
namespace {

constexpr wchar_t kContextOverlayClass[] = L"VisualContextOverlayWindow";
constexpr DWORD kExcludeFromCapture = 0x00000011;

std::uint32_t premultiplied_argb(std::uint8_t alpha, std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept {
    const auto scale = [alpha](std::uint8_t channel) -> std::uint8_t {
        return static_cast<std::uint8_t>((static_cast<unsigned>(channel) * alpha + 127U) / 255U);
    };
    return (static_cast<std::uint32_t>(alpha) << 24U)
        | (static_cast<std::uint32_t>(scale(red)) << 16U)
        | (static_cast<std::uint32_t>(scale(green)) << 8U)
        | static_cast<std::uint32_t>(scale(blue));
}

bool same_rect(const RECT& a, const RECT& b) noexcept {
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

} // namespace

ContextOverlay::~ContextOverlay() {
    destroy();
}

bool ContextOverlay::create(HINSTANCE instance, const RECT& source_monitor_rect) {
    destroy();
    source_monitor_rect_ = source_monitor_rect;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = &ContextOverlay::window_proc;
    wc.lpszClassName = kContextOverlayClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kContextOverlayClass,
        L"Visual Detail View indicator",
        WS_POPUP,
        source_monitor_rect.left,
        source_monitor_rect.top,
        1,
        1,
        nullptr,
        nullptr,
        instance,
        this);
    if (!hwnd_) return false;

    excluded_from_capture_ = SetWindowDisplayAffinity(hwnd_, kExcludeFromCapture) != FALSE;
    if (!excluded_from_capture_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }
    return true;
}

void ContextOverlay::update(const visual::core::ScreenRect& viewport, double zoom, bool enabled, bool shade) {
    if (!hwnd_ || !excluded_from_capture_ || !enabled || visual::core::near_zoom(zoom, 1.0) || !viewport.valid()) {
        hide();
        return;
    }

    RECT target{
        static_cast<LONG>(std::lround(std::max(viewport.left, static_cast<double>(source_monitor_rect_.left)))),
        static_cast<LONG>(std::lround(std::max(viewport.top, static_cast<double>(source_monitor_rect_.top)))),
        static_cast<LONG>(std::lround(std::min(viewport.right, static_cast<double>(source_monitor_rect_.right)))),
        static_cast<LONG>(std::lround(std::min(viewport.bottom, static_cast<double>(source_monitor_rect_.bottom))))
    };
    const int width = target.right - target.left;
    const int height = target.bottom - target.top;
    if (width < 12 || height < 12) {
        hide();
        return;
    }

    if (visible_ && same_rect(target, last_rect_) && shade == last_shade_) return;
    if (!ensure_surface(width, height)) {
        hide();
        return;
    }
    draw_surface(width, height, shade);

    HDC screen_dc = GetDC(nullptr);
    if (!screen_dc) return;
    POINT destination{target.left, target.top};
    POINT source{0, 0};
    SIZE size{width, height};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    SetWindowPos(hwnd_, HWND_TOPMOST, target.left, target.top, width, height,
                 SWP_NOACTIVATE | SWP_NOSENDCHANGING | SWP_NOOWNERZORDER);
    const BOOL updated = UpdateLayeredWindow(hwnd_, screen_dc, &destination, &size, memory_dc_, &source,
                                             0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screen_dc);
    if (!updated) {
        hide();
        return;
    }

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    visible_ = true;
    last_rect_ = target;
    last_shade_ = shade;
}

void ContextOverlay::hide() noexcept {
    if (hwnd_ && visible_) ShowWindow(hwnd_, SW_HIDE);
    visible_ = false;
    last_rect_ = {};
}

void ContextOverlay::destroy() noexcept {
    hide();
    release_surface();
    if (hwnd_) DestroyWindow(hwnd_);
    hwnd_ = nullptr;
    excluded_from_capture_ = false;
}

bool ContextOverlay::ensure_surface(int width, int height) {
    if (memory_dc_ && bitmap_ && bitmap_width_ == width && bitmap_height_ == height && bitmap_bits_) return true;
    release_surface();

    HDC screen_dc = GetDC(nullptr);
    if (!screen_dc) return false;
    memory_dc_ = CreateCompatibleDC(screen_dc);
    ReleaseDC(nullptr, screen_dc);
    if (!memory_dc_) return false;

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap_ = CreateDIBSection(memory_dc_, &info, DIB_RGB_COLORS, &bitmap_bits_, nullptr, 0);
    if (!bitmap_ || !bitmap_bits_) {
        release_surface();
        return false;
    }
    old_bitmap_ = SelectObject(memory_dc_, bitmap_);
    bitmap_width_ = width;
    bitmap_height_ = height;
    return true;
}

void ContextOverlay::draw_surface(int width, int height, bool shade) {
    auto* pixels = static_cast<std::uint32_t*>(bitmap_bits_);
    const std::uint32_t fill = shade ? premultiplied_argb(48, 220, 220, 220) : 0U;
    const std::uint32_t outer = premultiplied_argb(235, 0, 0, 0);
    const std::uint32_t inner = premultiplied_argb(255, 255, 255, 255);
    const int outer_width = 7;
    const int inner_width = 3;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int edge = std::min(std::min(x, width - 1 - x), std::min(y, height - 1 - y));
            std::uint32_t color = fill;
            if (edge < outer_width) color = outer;
            if (edge < inner_width) color = inner;
            pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] = color;
        }
    }
}

void ContextOverlay::release_surface() noexcept {
    if (memory_dc_ && old_bitmap_) SelectObject(memory_dc_, old_bitmap_);
    old_bitmap_ = nullptr;
    if (bitmap_) DeleteObject(bitmap_);
    bitmap_ = nullptr;
    bitmap_bits_ = nullptr;
    if (memory_dc_) DeleteDC(memory_dc_);
    memory_dc_ = nullptr;
    bitmap_width_ = 0;
    bitmap_height_ = 0;
}

LRESULT CALLBACK ContextOverlay::window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    switch (message) {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_ERASEBKGND:
        return 1;
    default:
        return DefWindowProcW(hwnd, message, w_param, l_param);
    }
}

} // namespace visual::ui
