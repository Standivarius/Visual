#include "core/view_controller.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace visual::core;

struct Failure { std::string name; std::string message; };

static void require(bool ok, const std::string& name, const std::string& message, std::vector<Failure>& failures) {
    if (!ok) failures.push_back({name, message});
}

static bool near(double a, double b, double eps = 1e-9) { return std::abs(a - b) <= eps; }

static PoiCandidate caret(double x, double y, std::uint64_t qpc) {
    PoiCandidate c{};
    c.kind = PoiKind::Caret;
    c.source = PoiSource::UiaTextPattern2Caret;
    c.confidence = PoiConfidence::High;
    c.screen_rect = {x, y, x + 4, y + 20};
    c.timestamp_qpc = qpc;
    return c;
}

int main() {
    std::vector<Failure> failures;
    constexpr std::uint64_t freq = 1000;
    const ScreenRect source{0, 0, 1920, 1080};

    ViewController controller;
    auto update = controller.update(source, 2, true, {caret(1400, 700, 1000)}, 1010, freq);
    update = controller.update(source, 2, true, {caret(1500, 760, 1020)}, 1030, freq);
    const ScreenRect before = update.viewport;

    const auto normal = controller.update(source, 1, true, {caret(200, 100, 1040)}, 1050, freq);
    require(near(normal.viewport.left, source.left) && near(normal.viewport.top, source.top), "normal_full_source", "1x should show full source", failures);
    require(near(normal.viewport.right, source.right) && near(normal.viewport.bottom, source.bottom), "normal_full_source", "1x should show full source", failures);

    const auto normal_hold = controller.update(source, 1, true, {caret(100, 50, 1060)}, 1070, freq);
    require(normal_hold.action == ViewportAction::Hold, "normal_hold", "1x should stay stable", failures);
    require(near(normal_hold.viewport.left, 0.0) && near(normal_hold.viewport.right, 1920.0), "normal_hold", "1x drifted", failures);

    const auto restored = controller.update(source, 2, true, {caret(100, 50, 1080)}, 1090, freq);
    require(restored.restored_previous_view, "restored_flag", "return to prior magnification should be identified as restore", failures);
    require(near(restored.viewport.left, before.left) && near(restored.viewport.top, before.top)
        && near(restored.viewport.right, before.right) && near(restored.viewport.bottom, before.bottom),
        "exact_restore", "previous magnified viewport was not restored exactly", failures);

    const auto next = controller.update(source, 2, true, {caret(100, 50, 1100)}, 1110, freq);
    require(!next.restored_previous_view, "restore_once", "restore flag should only apply to transition frame", failures);
    require(next.action == ViewportAction::Pan || next.action == ViewportAction::Jump || next.action == ViewportAction::Hold,
        "tracking_resumes", "tracking should resume after restore", failures);

    if (!failures.empty()) {
        std::cerr << "view_return_tests FAILED: " << failures.size() << " failure(s)\n";
        for (const auto& f : failures) std::cerr << "  " << f.name << ": " << f.message << "\n";
        return 1;
    }
    std::cout << "view_return_tests PASSED\n";
    return 0;
}
