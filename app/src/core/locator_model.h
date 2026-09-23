#pragma once

#include "poi_evidence.h"

#include <algorithm>

namespace visual::core {

struct LocatorTarget {
    bool visible{};
    PoiKind kind{PoiKind::Focus};
    double left{};
    double top{};
    double right{};
    double bottom{};
};

[[nodiscard]] inline LocatorTarget make_locator_target(
    const PoiCandidate& poi,
    const ScreenRect& viewport) noexcept {

    LocatorTarget target{};
    target.kind = poi.kind;
    if (!poi.usable() || !viewport.valid()) return target;

    const auto& r = poi.screen_rect;
    const bool intersects = r.right > viewport.left && r.left < viewport.right
        && r.bottom > viewport.top && r.top < viewport.bottom;
    if (!intersects) return target;

    target.left = std::clamp((r.left - viewport.left) / viewport.width(), 0.0, 1.0);
    target.top = std::clamp((r.top - viewport.top) / viewport.height(), 0.0, 1.0);
    target.right = std::clamp((r.right - viewport.left) / viewport.width(), 0.0, 1.0);
    target.bottom = std::clamp((r.bottom - viewport.top) / viewport.height(), 0.0, 1.0);
    target.visible = target.right >= target.left && target.bottom >= target.top;
    return target;
}

} // namespace visual::core
