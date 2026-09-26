#include "core/settings_model.h"

#include <iostream>
#include <string>
#include <vector>

using namespace visual::core;

struct Failure { std::string name; std::string message; };

static void require(bool ok, const std::string& name, const std::string& message, std::vector<Failure>& failures) {
    if (!ok) failures.push_back({name, message});
}

int main() {
    std::vector<Failure> failures;

    require(supported_zoom(1.0), "zoom_1", "1x should be supported", failures);
    require(supported_zoom(1.5), "zoom_1_5", "1.5x should be supported", failures);
    require(supported_zoom(2.0), "zoom_2", "2x should be supported", failures);
    require(supported_zoom(3.0), "zoom_3", "3x should be supported", failures);
    require(supported_zoom(4.0), "zoom_4", "4x should be supported", failures);
    require(!supported_zoom(2.5), "zoom_2_5", "2.5x should not be supported", failures);
    require(near_zoom(sanitize_zoom(1.50001), 1.5), "zoom_snap", "near 1.5 should sanitize to 1.5", failures);
    require(near_zoom(sanitize_zoom(9.0), 2.0), "zoom_fallback", "invalid zoom should fall back to 2x", failures);

    VisualSettings settings{};
    settings.follow_pointer = false;
    settings.show_caret_locator = false;
    require(!should_follow(PoiKind::Pointer, settings), "follow_pointer_off", "pointer follow toggle ignored", failures);
    require(should_follow(PoiKind::Caret, settings), "follow_caret_default", "caret follow should remain enabled", failures);
    require(!should_show_locator(PoiKind::Caret, settings), "caret_marker_off", "caret locator toggle ignored", failures);
    require(should_show_locator(PoiKind::Pointer, settings), "pointer_marker_default", "pointer locator should remain enabled", failures);

    settings.context_monitor_device = L"DISPLAY_A";
    settings.detail_monitor_device = L"DISPLAY_B";
    settings.reference_monitor_device = L"DISPLAY_C";
    require(monitor_roles_valid(settings), "roles_unique", "unique roles should be valid", failures);
    settings.reference_monitor_device = L"DISPLAY_B";
    require(!monitor_roles_valid(settings), "roles_duplicate", "reference must not duplicate detail", failures);

    require(sanitize_visual_mode(99) == VisualMode::Normal, "mode_fallback", "invalid visual mode should fall back to Normal", failures);
    require(sanitize_visual_mode(static_cast<int>(VisualMode::Inverted)) == VisualMode::Inverted,
            "mode_inverted", "valid visual mode changed", failures);

    if (!failures.empty()) {
        std::cerr << "settings_model_tests FAILED: " << failures.size() << " failure(s)\n";
        for (const auto& f : failures) std::cerr << "  " << f.name << ": " << f.message << "\n";
        return 1;
    }
    std::cout << "settings_model_tests PASSED\n";
    return 0;
}
