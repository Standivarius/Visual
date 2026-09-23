#pragma once

#include <cmath>
#include <optional>

namespace visual::core {

struct ScreenPoint {
    long x{};
    long y{};
};

struct MonitorRect {
    long left{};
    long top{};
    long right{};
    long bottom{};

    [[nodiscard]] long width() const noexcept { return right - left; }
    [[nodiscard]] long height() const noexcept { return bottom - top; }
    [[nodiscard]] bool valid() const noexcept { return width() > 0 && height() > 0; }
    [[nodiscard]] bool contains(ScreenPoint p) const noexcept {
        return valid() && p.x >= left && p.x < right && p.y >= top && p.y < bottom;
    }
};

struct MonitorLocalPoint {
    long x{};
    long y{};
};

struct NormalizedPoint {
    double x{};
    double y{};
};

[[nodiscard]] inline std::optional<MonitorLocalPoint> to_monitor_local(
    ScreenPoint screen,
    const MonitorRect& monitor) noexcept {
    if (!monitor.contains(screen)) return std::nullopt;
    return MonitorLocalPoint{screen.x - monitor.left, screen.y - monitor.top};
}

[[nodiscard]] inline std::optional<NormalizedPoint> to_normalized(
    ScreenPoint screen,
    const MonitorRect& monitor) noexcept {
    const auto local = to_monitor_local(screen, monitor);
    if (!local) return std::nullopt;
    const long w = monitor.width();
    const long h = monitor.height();
    return NormalizedPoint{
        w > 1 ? static_cast<double>(local->x) / static_cast<double>(w - 1) : 0.0,
        h > 1 ? static_cast<double>(local->y) / static_cast<double>(h - 1) : 0.0,
    };
}

[[nodiscard]] inline std::optional<ScreenPoint> from_normalized(
    NormalizedPoint normalized,
    const MonitorRect& monitor) noexcept {
    if (!monitor.valid() || !std::isfinite(normalized.x) || !std::isfinite(normalized.y)) return std::nullopt;
    if (normalized.x < 0.0 || normalized.x > 1.0 || normalized.y < 0.0 || normalized.y > 1.0) return std::nullopt;
    const long w = monitor.width();
    const long h = monitor.height();
    return ScreenPoint{
        monitor.left + (w > 1 ? static_cast<long>(std::llround(normalized.x * static_cast<double>(w - 1))) : 0),
        monitor.top + (h > 1 ? static_cast<long>(std::llround(normalized.y * static_cast<double>(h - 1))) : 0),
    };
}

} // namespace visual::core
