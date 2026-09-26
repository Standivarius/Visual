#pragma once

#include "poi_evidence.h"

#include <array>
#include <cmath>
#include <string>

namespace visual::core {

enum class VisualMode : int {
    Normal = 0,
    HighContrast = 1,
    Inverted = 2,
    Grayscale = 3,
};

struct VisualSettings {
    double zoom{2.0};
    bool tracking_enabled{true};
    bool follow_pointer{true};
    bool follow_caret{true};
    bool follow_focus{true};
    bool show_pointer_locator{true};
    bool show_caret_locator{true};
    bool show_focus_locator{true};
    bool show_context_indicator{true};
    bool shade_context_indicator{true};
    VisualMode visual_mode{VisualMode::Normal};
    std::wstring context_monitor_device{};
    std::wstring detail_monitor_device{};
    std::wstring reference_monitor_device{};
};

inline constexpr std::array<double, 5> kSupportedZoomLevels{1.0, 1.5, 2.0, 3.0, 4.0};

[[nodiscard]] inline bool near_zoom(double lhs, double rhs) noexcept {
    return std::isfinite(lhs) && std::isfinite(rhs) && std::abs(lhs - rhs) < 0.001;
}

[[nodiscard]] inline bool supported_zoom(double zoom) noexcept {
    if (!std::isfinite(zoom)) return false;
    for (const double candidate : kSupportedZoomLevels) {
        if (near_zoom(zoom, candidate)) return true;
    }
    return false;
}

[[nodiscard]] inline double sanitize_zoom(double zoom, double fallback = 2.0) noexcept {
    if (supported_zoom(zoom)) {
        for (const double candidate : kSupportedZoomLevels) {
            if (near_zoom(zoom, candidate)) return candidate;
        }
    }
    return supported_zoom(fallback) ? fallback : 2.0;
}

[[nodiscard]] inline VisualMode sanitize_visual_mode(int value) noexcept {
    switch (value) {
    case static_cast<int>(VisualMode::Normal): return VisualMode::Normal;
    case static_cast<int>(VisualMode::HighContrast): return VisualMode::HighContrast;
    case static_cast<int>(VisualMode::Inverted): return VisualMode::Inverted;
    case static_cast<int>(VisualMode::Grayscale): return VisualMode::Grayscale;
    default: return VisualMode::Normal;
    }
}

[[nodiscard]] inline bool should_follow(PoiKind kind, const VisualSettings& settings) noexcept {
    switch (kind) {
    case PoiKind::Pointer: return settings.follow_pointer;
    case PoiKind::Caret: return settings.follow_caret;
    case PoiKind::Focus: return settings.follow_focus;
    case PoiKind::ExplicitTarget: return true;
    default: return true;
    }
}

[[nodiscard]] inline bool should_show_locator(PoiKind kind, const VisualSettings& settings) noexcept {
    switch (kind) {
    case PoiKind::Pointer: return settings.show_pointer_locator;
    case PoiKind::Caret: return settings.show_caret_locator;
    case PoiKind::Focus: return settings.show_focus_locator;
    case PoiKind::ExplicitTarget: return settings.show_focus_locator;
    default: return true;
    }
}

[[nodiscard]] inline bool monitor_roles_valid(const VisualSettings& settings) noexcept {
    if (!settings.context_monitor_device.empty() && settings.context_monitor_device == settings.detail_monitor_device) return false;
    if (!settings.reference_monitor_device.empty()) {
        if (settings.reference_monitor_device == settings.context_monitor_device) return false;
        if (settings.reference_monitor_device == settings.detail_monitor_device) return false;
    }
    return true;
}

} // namespace visual::core
