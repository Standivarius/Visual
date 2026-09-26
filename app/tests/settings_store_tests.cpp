#include "settings_store.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

using visual::core::VisualMode;
using visual::core::VisualSettings;

struct Failure { std::string name; std::string message; };

static void require(bool ok, const std::string& name, const std::string& message, std::vector<Failure>& failures) {
    if (!ok) failures.push_back({name, message});
}

int main() {
    std::vector<Failure> failures;
    const auto path = std::filesystem::temp_directory_path() /
        (L"visual_settings_store_tests_" + std::to_wstring(GetCurrentProcessId()) + L".ini");
    std::error_code ec;
    std::filesystem::remove(path, ec);

    VisualSettings expected{};
    expected.zoom = 1.5;
    expected.tracking_enabled = false;
    expected.follow_pointer = false;
    expected.follow_caret = true;
    expected.follow_focus = false;
    expected.show_pointer_locator = false;
    expected.show_caret_locator = true;
    expected.show_focus_locator = false;
    expected.show_context_indicator = true;
    expected.shade_context_indicator = false;
    expected.visual_mode = VisualMode::Inverted;
    expected.context_monitor_device = L"DISPLAY_A";
    expected.detail_monitor_device = L"DISPLAY_B";
    expected.reference_monitor_device = L"DISPLAY_C";

    require(visual::settings::save_settings(path, expected), "save", "settings save failed", failures);
    require(std::filesystem::exists(path), "file_exists", "settings file was not created", failures);
    const auto actual = visual::settings::load_settings(path);

    require(visual::core::near_zoom(actual.zoom, expected.zoom), "zoom", "zoom did not round-trip", failures);
    require(actual.tracking_enabled == expected.tracking_enabled, "tracking", "tracking did not round-trip", failures);
    require(actual.follow_pointer == expected.follow_pointer, "follow_pointer", "pointer follow did not round-trip", failures);
    require(actual.follow_caret == expected.follow_caret, "follow_caret", "caret follow did not round-trip", failures);
    require(actual.follow_focus == expected.follow_focus, "follow_focus", "focus follow did not round-trip", failures);
    require(actual.show_pointer_locator == expected.show_pointer_locator, "pointer_marker", "pointer marker did not round-trip", failures);
    require(actual.show_caret_locator == expected.show_caret_locator, "caret_marker", "caret marker did not round-trip", failures);
    require(actual.show_focus_locator == expected.show_focus_locator, "focus_marker", "focus marker did not round-trip", failures);
    require(actual.show_context_indicator == expected.show_context_indicator, "indicator", "context indicator did not round-trip", failures);
    require(actual.shade_context_indicator == expected.shade_context_indicator, "shade", "context shade did not round-trip", failures);
    require(actual.visual_mode == expected.visual_mode, "mode", "visual mode did not round-trip", failures);
    require(actual.context_monitor_device == expected.context_monitor_device, "context_monitor", "context monitor did not round-trip", failures);
    require(actual.detail_monitor_device == expected.detail_monitor_device, "detail_monitor", "detail monitor did not round-trip", failures);
    require(actual.reference_monitor_device == expected.reference_monitor_device, "reference_monitor", "reference monitor did not round-trip", failures);

    std::filesystem::remove(path, ec);

    if (!failures.empty()) {
        std::cerr << "settings_store_tests FAILED: " << failures.size() << " failure(s)\n";
        for (const auto& f : failures) std::cerr << "  " << f.name << ": " << f.message << "\n";
        return 1;
    }
    std::cout << "settings_store_tests PASSED\n";
    return 0;
}
