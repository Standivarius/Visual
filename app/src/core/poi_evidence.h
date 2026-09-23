#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace visual::core {

enum class PoiKind { Pointer, Caret, Focus, ExplicitTarget };
enum class PoiSource {
    UiaTextPattern2Caret,
    UiaTextPatternSelection,
    Win32Caret,
    UiaFocus,
    Win32Focus,
    Pointer,
    ExplicitUserTarget,
};
enum class PoiConfidence { Low = 1, Medium = 2, High = 3 };

struct ScreenRect {
    double left{};
    double top{};
    double right{};
    double bottom{};
    [[nodiscard]] double width() const noexcept { return right - left; }
    [[nodiscard]] double height() const noexcept { return bottom - top; }
    [[nodiscard]] bool valid() const noexcept { return right > left && bottom > top; }
};

struct PoiCandidate {
    PoiKind kind{PoiKind::Focus};
    PoiSource source{PoiSource::UiaFocus};
    PoiConfidence confidence{PoiConfidence::Low};
    ScreenRect screen_rect{};
    std::uint64_t timestamp_qpc{};
    std::uint32_t process_id{};
    std::uint64_t identity{};
    [[nodiscard]] bool usable() const noexcept { return screen_rect.valid() && timestamp_qpc != 0; }
};

struct PoiSelectionPolicy {
    double max_caret_age_ms{250.0};
    double max_focus_age_ms{500.0};
    double max_pointer_age_ms{100.0};
};

[[nodiscard]] inline int source_priority(PoiSource s) noexcept {
    switch (s) {
    case PoiSource::ExplicitUserTarget: return 700;
    // A deliberately moving pointer is direct current user intent. Its evidence is deliberately
    // short-lived (default 100 ms), so it temporarily wins and then naturally yields back to caret/focus.
    case PoiSource::Pointer: return 650;
    case PoiSource::UiaTextPattern2Caret: return 620;
    case PoiSource::UiaTextPatternSelection: return 610;
    case PoiSource::Win32Caret: return 600;
    case PoiSource::UiaFocus: return 400;
    case PoiSource::Win32Focus: return 300;
    }
    return 0;
}

[[nodiscard]] inline double max_age_ms_for(const PoiCandidate& c, const PoiSelectionPolicy& p) noexcept {
    switch (c.kind) {
    case PoiKind::Caret: return p.max_caret_age_ms;
    case PoiKind::Focus: return p.max_focus_age_ms;
    case PoiKind::Pointer: return p.max_pointer_age_ms;
    case PoiKind::ExplicitTarget: return 1000.0;
    }
    return 0.0;
}

[[nodiscard]] inline bool is_fresh(
    const PoiCandidate& c,
    std::uint64_t now_qpc,
    std::uint64_t qpc_frequency,
    const PoiSelectionPolicy& policy) noexcept {
    if (!c.usable() || qpc_frequency == 0 || now_qpc < c.timestamp_qpc) return false;
    const auto ticks = now_qpc - c.timestamp_qpc;
    const double age_ms = 1000.0 * static_cast<double>(ticks) / static_cast<double>(qpc_frequency);
    return age_ms <= max_age_ms_for(c, policy);
}

[[nodiscard]] inline std::optional<PoiCandidate> select_best_poi(
    const std::vector<PoiCandidate>& candidates,
    std::uint64_t now_qpc,
    std::uint64_t qpc_frequency,
    const PoiSelectionPolicy& policy = {}) noexcept {
    std::optional<PoiCandidate> best;
    for (const auto& c : candidates) {
        if (!is_fresh(c, now_qpc, qpc_frequency, policy)) continue;
        if (!best) { best = c; continue; }
        const int cr = source_priority(c.source) * 10 + static_cast<int>(c.confidence);
        const int br = source_priority(best->source) * 10 + static_cast<int>(best->confidence);
        if (cr > br || (cr == br && c.timestamp_qpc > best->timestamp_qpc)) best = c;
    }
    return best;
}

} // namespace visual::core
