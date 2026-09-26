#include "core/coordinate_model.h"
#include "core/poi_evidence.h"
#include "core/viewport_policy.h"
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

static bool near(double a, double b, double eps = 1e-9) {
    return std::abs(a - b) <= eps;
}

static PoiCandidate make_candidate(PoiKind kind, PoiSource source, PoiConfidence confidence,
                                   ScreenRect rect, std::uint64_t qpc) {
    PoiCandidate c{};
    c.kind = kind;
    c.source = source;
    c.confidence = confidence;
    c.screen_rect = rect;
    c.timestamp_qpc = qpc;
    c.process_id = 1234;
    return c;
}

static void test_coordinate_roundtrip(std::vector<Failure>& failures) {
    const std::string name = "coordinate_roundtrip";
    const MonitorRect monitor{-1920, 0, 0, 1080};
    const ScreenPoint p{-960, 540};
    const auto n = to_normalized(p, monitor);
    require(n.has_value(), name, "normalization failed", failures);
    if (!n) return;
    const auto back = from_normalized(*n, monitor);
    require(back.has_value(), name, "roundtrip failed", failures);
    if (back) require(back->x == p.x && back->y == p.y, name, "roundtrip changed point", failures);
}

static void test_poi_fallback_selection(std::vector<Failure>& failures) {
    const std::string name = "poi_fallback_selection";
    const std::uint64_t freq = 1000;
    const std::uint64_t now = 10000;
    std::vector<PoiCandidate> candidates{
        make_candidate(PoiKind::Caret, PoiSource::UiaTextPattern2Caret, PoiConfidence::High,
                       {900, 300, 905, 320}, now - 800),
        make_candidate(PoiKind::Caret, PoiSource::Win32Caret, PoiConfidence::High,
                       {850, 300, 855, 320}, now - 20),
        make_candidate(PoiKind::Focus, PoiSource::UiaFocus, PoiConfidence::High,
                       {700, 250, 1000, 500}, now - 10),
    };
    const auto selected = select_best_poi(candidates, now, freq);
    require(selected.has_value(), name, "no POI selected", failures);
    if (selected) require(selected->source == PoiSource::Win32Caret, name, "fresh Win32 caret should beat stale UIA caret and focus", failures);
}

static void test_poi_to_viewport_pipeline(std::vector<Failure>& failures) {
    const std::string name = "poi_to_viewport_pipeline";
    const std::uint64_t freq = 1000;
    const std::uint64_t now = 5000;
    std::vector<PoiCandidate> candidates{
        make_candidate(PoiKind::Focus, PoiSource::UiaFocus, PoiConfidence::High,
                       {1200, 300, 1400, 600}, now - 10),
        make_candidate(PoiKind::Caret, PoiSource::UiaTextPattern2Caret, PoiConfidence::High,
                       {940, 350, 944, 370}, now - 5),
    };

    const auto selected = select_best_poi(candidates, now, freq);
    require(selected.has_value(), name, "caret should be selected", failures);
    if (!selected) return;

    const ScreenRect source{0, 0, 1920, 1080};
    const ScreenRect current{0, 0, 1000, 600};
    const auto decision = decide_viewport(source, current, selected->screen_rect);
    require(decision.action == ViewportAction::Pan, name, "caret just outside comfort area should pan", failures);
    require(decision.applied_dx > 0.0, name, "viewport should move right", failures);
    require(decision.applied_dy == 0.0, name, "viewport should not move vertically", failures);
}

static void test_safe_caret_holds(std::vector<Failure>& failures) {
    const std::string name = "safe_caret_holds";
    const PoiCandidate caret = make_candidate(PoiKind::Caret, PoiSource::Win32Caret, PoiConfidence::High,
                                              {500, 300, 504, 320}, 1000);
    const auto selected = select_best_poi({caret}, 1010, 1000);
    require(selected.has_value(), name, "caret missing", failures);
    if (!selected) return;
    const auto decision = decide_viewport({0,0,1920,1080}, {0,0,1000,600}, selected->screen_rect);
    require(decision.action == ViewportAction::Hold, name, "comfortable caret should not move viewport", failures);
}

static void test_stale_evidence_does_not_move(std::vector<Failure>& failures) {
    const std::string name = "stale_evidence_does_not_move";
    const auto stale = make_candidate(PoiKind::Caret, PoiSource::Win32Caret, PoiConfidence::High,
                                      {1700, 800, 1710, 820}, 1000);
    const auto selected = select_best_poi({stale}, 2000, 1000);
    require(!selected.has_value(), name, "stale caret should produce no target", failures);
}

static void test_view_controller_zoom_anchors_selected_poi(std::vector<Failure>& failures) {
    const std::string name = "view_controller_zoom_anchor";
    ViewController controller;
    const ScreenRect source{0, 0, 1920, 1080};
    const auto caret = make_candidate(PoiKind::Caret, PoiSource::UiaTextPattern2Caret, PoiConfidence::High,
                                      {1000, 500, 1004, 520}, 1000);
    const auto first = controller.update(source, 2, true, {caret}, 1010, 1000);
    require(first.selected_poi.has_value(), name, "caret should be selected", failures);
    require(near(first.viewport.width(), 960.0), name, "2x viewport width should be half source width", failures);
    require(near(first.viewport.height(), 540.0), name, "2x viewport height should be half source height", failures);
    const double center_x = (first.viewport.left + first.viewport.right) * 0.5;
    require(std::abs(center_x - 1002.0) < 1.0, name, "initial zoom should preserve selected POI near viewport center", failures);
}

static void test_fractional_and_three_x_zoom_geometry(std::vector<Failure>& failures) {
    const std::string name = "fractional_and_three_x_zoom_geometry";
    constexpr std::uint64_t freq = 1000;
    const ScreenRect source{0, 0, 1920, 1080};
    const auto target = make_candidate(PoiKind::Caret, PoiSource::Win32Caret, PoiConfidence::High,
                                       {960, 540, 964, 560}, 1000);

    ViewController one_and_half;
    const auto fractional = one_and_half.update(source, 1.5, true, {target}, 1010, freq);
    require(near(fractional.viewport.width(), 1280.0), name, "1.5x viewport width should be 1280", failures);
    require(near(fractional.viewport.height(), 720.0), name, "1.5x viewport height should be 720", failures);

    ViewController three_x;
    const auto three = three_x.update(source, 3.0, true, {target}, 1010, freq);
    require(near(three.viewport.width(), 640.0), name, "3x viewport width should be 640", failures);
    require(near(three.viewport.height(), 360.0), name, "3x viewport height should be 360", failures);
}

static void test_view_controller_hold_then_pan_without_drift(std::vector<Failure>& failures) {
    const std::string name = "view_controller_hold_pan";
    ViewController controller;
    const ScreenRect source{0, 0, 2000, 1000};
    const auto center = make_candidate(PoiKind::Caret, PoiSource::Win32Caret, PoiConfidence::High,
                                       {500, 240, 504, 260}, 1000);
    const auto first = controller.update(source, 2, true, {center}, 1010, 1000);
    const auto second = controller.update(source, 2, true, {center}, 1020, 1000);
    require(second.action == ViewportAction::Hold, name, "stable comfortable caret should hold", failures);
    require(near(first.viewport.left, second.viewport.left) && near(first.viewport.top, second.viewport.top), name, "hold should not drift", failures);

    const auto far_right = make_candidate(PoiKind::Caret, PoiSource::Win32Caret, PoiConfidence::High,
                                          {1300, 240, 1304, 260}, 1030);
    const auto third = controller.update(source, 2, true, {far_right}, 1040, 1000);
    require(third.action == ViewportAction::Pan || third.action == ViewportAction::Jump, name, "distant caret should move viewport", failures);
    require(third.viewport.left > second.viewport.left, name, "viewport should move right", failures);
}

static void test_view_controller_tracking_disabled_holds(std::vector<Failure>& failures) {
    const std::string name = "view_controller_tracking_disabled";
    ViewController controller;
    const ScreenRect source{0, 0, 1920, 1080};
    const auto initial = make_candidate(PoiKind::Caret, PoiSource::Win32Caret, PoiConfidence::High,
                                        {600, 300, 604, 320}, 1000);
    const auto first = controller.update(source, 2, true, {initial}, 1010, 1000);
    const auto far = make_candidate(PoiKind::Caret, PoiSource::Win32Caret, PoiConfidence::High,
                                    {1700, 900, 1704, 920}, 1020);
    const auto second = controller.update(source, 2, false, {far}, 1030, 1000);
    require(second.action == ViewportAction::Hold, name, "disabled tracking should hold", failures);
    require(near(first.viewport.left, second.viewport.left) && near(first.viewport.top, second.viewport.top), name, "disabled tracking changed viewport", failures);
}

int main() {
    std::vector<Failure> failures;
    test_coordinate_roundtrip(failures);
    test_poi_fallback_selection(failures);
    test_poi_to_viewport_pipeline(failures);
    test_safe_caret_holds(failures);
    test_stale_evidence_does_not_move(failures);
    test_view_controller_zoom_anchors_selected_poi(failures);
    test_fractional_and_three_x_zoom_geometry(failures);
    test_view_controller_hold_then_pan_without_drift(failures);
    test_view_controller_tracking_disabled_holds(failures);

    if (!failures.empty()) {
        std::cerr << "core_pipeline_tests FAILED: " << failures.size() << " failure(s)\n";
        for (const auto& f : failures) std::cerr << "  " << f.name << ": " << f.message << "\n";
        return 1;
    }
    std::cout << "core_pipeline_tests PASSED\n";
    return 0;
}
