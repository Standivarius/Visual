#include "core/locator_model.h"

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

static PoiCandidate candidate(PoiKind kind, ScreenRect rect) {
    PoiCandidate c{};
    c.kind = kind;
    c.source = kind == PoiKind::Pointer ? PoiSource::Pointer : PoiSource::Win32Caret;
    c.confidence = PoiConfidence::High;
    c.screen_rect = rect;
    c.timestamp_qpc = 100;
    return c;
}

int main() {
    std::vector<Failure> failures;
    const ScreenRect viewport{100, 200, 1100, 700};

    {
        const auto t = make_locator_target(candidate(PoiKind::Caret, {600, 400, 604, 420}), viewport);
        require(t.visible, "caret_visible", "caret inside viewport should be visible", failures);
        require(t.kind == PoiKind::Caret, "caret_kind", "locator kind changed", failures);
        require(near(t.left, 0.5), "caret_left", "unexpected normalized caret x", failures);
        require(near(t.top, 0.4), "caret_top", "unexpected normalized caret y", failures);
    }

    {
        const auto t = make_locator_target(candidate(PoiKind::Pointer, {99, 300, 100, 301}), viewport);
        require(!t.visible, "outside_hidden", "POI outside viewport should not draw", failures);
    }

    {
        const auto t = make_locator_target(candidate(PoiKind::Focus, {50, 150, 300, 350}), viewport);
        require(t.visible, "partial_focus_visible", "partially visible focus rectangle should draw", failures);
        require(near(t.left, 0.0), "partial_focus_clamp_left", "left edge should clamp to zero", failures);
        require(near(t.top, 0.0), "partial_focus_clamp_top", "top edge should clamp to zero", failures);
    }

    {
        auto bad = candidate(PoiKind::Caret, {10, 10, 10, 20});
        const auto t = make_locator_target(bad, viewport);
        require(!t.visible, "invalid_hidden", "invalid POI geometry should not draw", failures);
    }

    if (!failures.empty()) {
        std::cerr << "locator_model_tests FAILED: " << failures.size() << " failure(s)\n";
        for (const auto& f : failures) std::cerr << "  " << f.name << ": " << f.message << "\n";
        return 1;
    }
    std::cout << "locator_model_tests PASSED\n";
    return 0;
}
