#pragma once

#include "poi_evidence.h"
#include "viewport_policy.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace visual::core {

struct ViewUpdate {
    ScreenRect viewport{};
    ViewportAction action{ViewportAction::Invalid};
    std::optional<PoiCandidate> selected_poi{};
    bool restored_previous_view{};
};

class ViewController {
public:
    [[nodiscard]] ViewUpdate update(
        const ScreenRect& source,
        double zoom,
        bool tracking_enabled,
        const std::vector<PoiCandidate>& candidates,
        std::uint64_t now_qpc,
        std::uint64_t qpc_frequency) noexcept {

        ViewUpdate result{};
        if (!source.valid() || !std::isfinite(zoom) || zoom <= 0.0) return result;

        const auto selected = select_best_poi(candidates, now_qpc, qpc_frequency, poi_policy_);
        result.selected_poi = selected;

        const double target_width = source.width() / static_cast<double>(zoom);
        const double target_height = source.height() / static_cast<double>(zoom);

        const bool entering_normal = viewport_.valid() && current_zoom_ > 1.0 && std::abs(zoom - 1.0) <= 0.001;
        if (entering_normal) {
            saved_magnified_viewport_ = viewport_;
            saved_magnified_zoom_ = current_zoom_;
            have_saved_magnified_view_ = true;
        }

        const bool saved_size_matches = have_saved_magnified_view_
            && std::abs(saved_magnified_viewport_.width() - target_width) <= 0.5
            && std::abs(saved_magnified_viewport_.height() - target_height) <= 0.5;
        const bool restoring_saved = std::abs(current_zoom_ - 1.0) <= 0.001
            && std::abs(zoom - saved_magnified_zoom_) <= 0.001
            && saved_size_matches;

        const bool size_changed = !viewport_.valid()
            || std::abs(current_zoom_ - zoom) > 0.001
            || std::abs(viewport_.width() - target_width) > 0.5
            || std::abs(viewport_.height() - target_height) > 0.5;

        if (size_changed) {
            if (restoring_saved) {
                const auto restored = clamp_viewport_to_source(saved_magnified_viewport_, source);
                if (restored.valid()
                    && std::abs(restored.width() - target_width) <= 0.5
                    && std::abs(restored.height() - target_height) <= 0.5) {
                    viewport_ = restored;
                    current_zoom_ = zoom;
                    have_saved_magnified_view_ = false;
                    result.viewport = viewport_;
                    result.action = ViewportAction::Hold;
                    result.restored_previous_view = true;
                    return result;
                }
                have_saved_magnified_view_ = false;
            }

            double center_x = source.left + source.width() * 0.5;
            double center_y = source.top + source.height() * 0.5;
            if (viewport_.valid() && !tracking_enabled && zoom > 1.0) {
                center_x = (viewport_.left + viewport_.right) * 0.5;
                center_y = (viewport_.top + viewport_.bottom) * 0.5;
            } else if (selected && zoom > 1.0) {
                center_x = (selected->screen_rect.left + selected->screen_rect.right) * 0.5;
                center_y = (selected->screen_rect.top + selected->screen_rect.bottom) * 0.5;
            }

            viewport_ = {
                center_x - target_width * 0.5,
                center_y - target_height * 0.5,
                center_x + target_width * 0.5,
                center_y + target_height * 0.5,
            };
            viewport_ = clamp_viewport_to_source(viewport_, source);
            current_zoom_ = zoom;
            result.viewport = viewport_;
            result.action = ViewportAction::Hold;
            return result;
        }

        // At 1x the full source is already visible. Keep it spatially stable and save movement policy
        // for the magnified state that will be restored later.
        if (std::abs(zoom - 1.0) <= 0.001) {
            result.viewport = viewport_;
            result.action = ViewportAction::Hold;
            return result;
        }

        if (!selected) {
            result.viewport = viewport_;
            result.action = ViewportAction::Hold;
            return result;
        }

        const auto decision = decide_viewport(source, viewport_, selected->screen_rect, tracking_enabled, viewport_config_);
        if (decision.action != ViewportAction::Invalid) viewport_ = decision.target_viewport;
        result.viewport = viewport_;
        result.action = decision.action;
        return result;
    }

    void set_viewport_config(const ViewportConfig& config) noexcept { viewport_config_ = config; }
    void set_poi_policy(const PoiSelectionPolicy& policy) noexcept { poi_policy_ = policy; }

    [[nodiscard]] const ScreenRect& viewport() const noexcept { return viewport_; }

private:
    double current_zoom_{};
    ScreenRect viewport_{};
    bool have_saved_magnified_view_{};
    double saved_magnified_zoom_{};
    ScreenRect saved_magnified_viewport_{};
    ViewportConfig viewport_config_{};
    PoiSelectionPolicy poi_policy_{};
};

} // namespace visual::core
