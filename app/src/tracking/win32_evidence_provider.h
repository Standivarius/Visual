#pragma once

#include "core/poi_evidence.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace visual::tracking {

class Win32EvidenceProvider {
public:
    [[nodiscard]] std::vector<core::PoiCandidate> sample(
        const core::ScreenRect& source,
        std::uint64_t now_qpc) noexcept {

        std::vector<core::PoiCandidate> out;
        sample_pointer(source, now_qpc, out);
        sample_gui_thread(source, now_qpc, out);
        return out;
    }

    [[nodiscard]] std::uint64_t last_pointer_movement_qpc() const noexcept {
        return last_pointer_movement_qpc_;
    }

private:
    [[nodiscard]] static bool intersects(const core::ScreenRect& a, const core::ScreenRect& b) noexcept {
        return a.valid() && b.valid()
            && a.right > b.left && a.left < b.right
            && a.bottom > b.top && a.top < b.bottom;
    }

    void sample_pointer(const core::ScreenRect& source, std::uint64_t now_qpc,
                        std::vector<core::PoiCandidate>& out) noexcept {
        POINT cursor{};
        if (!GetCursorPos(&cursor)) return;
        if (!have_last_pointer_ || cursor.x != last_pointer_.x || cursor.y != last_pointer_.y) {
            last_pointer_ = cursor;
            have_last_pointer_ = true;
            last_pointer_movement_qpc_ = now_qpc;
        }

        core::PoiCandidate pointer{};
        pointer.kind = core::PoiKind::Pointer;
        pointer.source = core::PoiSource::Pointer;
        pointer.confidence = core::PoiConfidence::High;
        pointer.screen_rect = {
            static_cast<double>(cursor.x), static_cast<double>(cursor.y),
            static_cast<double>(cursor.x + 1), static_cast<double>(cursor.y + 1)
        };
        pointer.timestamp_qpc = last_pointer_movement_qpc_;
        if (intersects(pointer.screen_rect, source)) out.push_back(pointer);
    }

    static void sample_gui_thread(const core::ScreenRect& source, std::uint64_t now_qpc,
                                  std::vector<core::PoiCandidate>& out) noexcept {
        HWND foreground = GetForegroundWindow();
        if (!foreground) return;

        DWORD foreground_pid = 0;
        const DWORD foreground_tid = GetWindowThreadProcessId(foreground, &foreground_pid);
        if (!foreground_tid) return;

        GUITHREADINFO gui{};
        gui.cbSize = sizeof(gui);
        if (!GetGUIThreadInfo(foreground_tid, &gui)) return;

        if (gui.hwndCaret) {
            POINT a{gui.rcCaret.left, gui.rcCaret.top};
            POINT b{gui.rcCaret.right, gui.rcCaret.bottom};
            if (ClientToScreen(gui.hwndCaret, &a) && ClientToScreen(gui.hwndCaret, &b)) {
                core::PoiCandidate caret{};
                caret.kind = core::PoiKind::Caret;
                caret.source = core::PoiSource::Win32Caret;
                caret.confidence = core::PoiConfidence::High;
                caret.screen_rect = {
                    static_cast<double>(a.x), static_cast<double>(a.y),
                    static_cast<double>(std::max(a.x + 1, b.x)),
                    static_cast<double>(std::max(a.y + 1, b.y))
                };
                caret.timestamp_qpc = now_qpc;
                caret.process_id = foreground_pid;
                if (intersects(caret.screen_rect, source)) out.push_back(caret);
            }
        }

        if (gui.hwndFocus) {
            RECT r{};
            if (GetWindowRect(gui.hwndFocus, &r)) {
                core::PoiCandidate focus{};
                focus.kind = core::PoiKind::Focus;
                focus.source = core::PoiSource::Win32Focus;
                focus.confidence = core::PoiConfidence::Medium;
                focus.screen_rect = {
                    static_cast<double>(r.left), static_cast<double>(r.top),
                    static_cast<double>(r.right), static_cast<double>(r.bottom)
                };
                focus.timestamp_qpc = now_qpc;
                focus.process_id = foreground_pid;
                if (intersects(focus.screen_rect, source)) out.push_back(focus);
            }
        }
    }

    POINT last_pointer_{};
    bool have_last_pointer_{};
    std::uint64_t last_pointer_movement_qpc_{};
};

} // namespace visual::tracking
