#pragma once

#include "poi_evidence.h"

#include <algorithm>
#include <cmath>

namespace visual::core {

enum class ViewportAction { Invalid, Hold, Pan, Jump };
enum class ViewportReason { InvalidGeometry, TrackingDisabled, PoiAlreadyComfortable, PoiOutsideComfortArea };

struct ViewportConfig {
    double margin_ratio_x{0.20};
    double margin_ratio_y{0.20};
    double jump_threshold_viewport_fraction{0.75};
};

struct ViewportDecision {
    ViewportAction action{ViewportAction::Invalid};
    ViewportReason reason{ViewportReason::InvalidGeometry};
    ScreenRect target_viewport{};
    double requested_dx{};
    double requested_dy{};
    double applied_dx{};
    double applied_dy{};
};

[[nodiscard]] inline ScreenRect translate(ScreenRect r, double dx, double dy) noexcept {
    return {r.left + dx, r.top + dy, r.right + dx, r.bottom + dy};
}

[[nodiscard]] inline bool contains(const ScreenRect& outer, const ScreenRect& inner) noexcept {
    return outer.valid() && inner.valid()
        && inner.left >= outer.left && inner.right <= outer.right
        && inner.top >= outer.top && inner.bottom <= outer.bottom;
}

[[nodiscard]] inline ScreenRect inset(const ScreenRect& r, double x, double y) noexcept {
    return {r.left + x, r.top + y, r.right - x, r.bottom - y};
}

[[nodiscard]] inline ScreenRect clamp_viewport_to_source(ScreenRect viewport, const ScreenRect& source) noexcept {
    if (!viewport.valid() || !source.valid()) return {};
    const double vw = viewport.width();
    const double vh = viewport.height();
    if (vw > source.width() || vh > source.height()) return {};
    double left = viewport.left;
    double top = viewport.top;
    if (left < source.left) left = source.left;
    if (top < source.top) top = source.top;
    if (left + vw > source.right) left = source.right - vw;
    if (top + vh > source.bottom) top = source.bottom - vh;
    return {left, top, left + vw, top + vh};
}

[[nodiscard]] inline bool viewport_config_valid(const ViewportConfig& c) noexcept {
    return std::isfinite(c.margin_ratio_x) && std::isfinite(c.margin_ratio_y)
        && std::isfinite(c.jump_threshold_viewport_fraction)
        && c.margin_ratio_x >= 0.0 && c.margin_ratio_x < 0.5
        && c.margin_ratio_y >= 0.0 && c.margin_ratio_y < 0.5
        && c.jump_threshold_viewport_fraction >= 0.0;
}

[[nodiscard]] inline ViewportDecision decide_viewport(
    const ScreenRect& source,
    const ScreenRect& current_viewport,
    const ScreenRect& poi,
    bool tracking_enabled = true,
    const ViewportConfig& config = {}) noexcept {

    ViewportDecision d{};
    d.target_viewport = current_viewport;

    if (!source.valid() || !current_viewport.valid() || !poi.valid() || !viewport_config_valid(config)
        || current_viewport.width() > source.width() || current_viewport.height() > source.height()) {
        return d;
    }

    const auto current = clamp_viewport_to_source(current_viewport, source);
    if (!current.valid()) return d;
    d.target_viewport = current;

    if (!tracking_enabled) {
        d.action = ViewportAction::Hold;
        d.reason = ViewportReason::TrackingDisabled;
        return d;
    }

    const auto comfort = inset(current, current.width() * config.margin_ratio_x, current.height() * config.margin_ratio_y);
    if (!comfort.valid()) return d;
    if (contains(comfort, poi)) {
        d.action = ViewportAction::Hold;
        d.reason = ViewportReason::PoiAlreadyComfortable;
        return d;
    }

    double dx = 0.0;
    double dy = 0.0;
    if (poi.width() > comfort.width()) dx = ((poi.left + poi.right) - (current.left + current.right)) * 0.5;
    else if (poi.left < comfort.left) dx = poi.left - comfort.left;
    else if (poi.right > comfort.right) dx = poi.right - comfort.right;

    if (poi.height() > comfort.height()) dy = ((poi.top + poi.bottom) - (current.top + current.bottom)) * 0.5;
    else if (poi.top < comfort.top) dy = poi.top - comfort.top;
    else if (poi.bottom > comfort.bottom) dy = poi.bottom - comfort.bottom;

    d.requested_dx = dx;
    d.requested_dy = dy;
    const auto clamped = clamp_viewport_to_source(translate(current, dx, dy), source);
    if (!clamped.valid()) return ViewportDecision{};

    d.target_viewport = clamped;
    d.applied_dx = clamped.left - current.left;
    d.applied_dy = clamped.top - current.top;
    d.reason = ViewportReason::PoiOutsideComfortArea;
    const double fx = std::abs(d.applied_dx) / current.width();
    const double fy = std::abs(d.applied_dy) / current.height();
    d.action = std::max(fx, fy) >= config.jump_threshold_viewport_fraction ? ViewportAction::Jump : ViewportAction::Pan;
    return d;
}

} // namespace visual::core
