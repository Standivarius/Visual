#include "core/poi_evidence.h"

#include <iostream>
#include <string>
#include <vector>

using namespace visual::core;

struct Failure { std::string name; std::string message; };

static void require(bool ok, const std::string& name, const std::string& message, std::vector<Failure>& failures) {
    if (!ok) failures.push_back({name, message});
}

static PoiCandidate make(PoiKind kind, PoiSource source, std::uint64_t qpc) {
    PoiCandidate c{};
    c.kind = kind;
    c.source = source;
    c.confidence = PoiConfidence::High;
    c.screen_rect = {100, 100, 110, 120};
    c.timestamp_qpc = qpc;
    return c;
}

int main() {
    std::vector<Failure> failures;
    constexpr std::uint64_t freq = 1000;

    {
        const std::uint64_t now = 10'000;
        const auto caret = make(PoiKind::Caret, PoiSource::UiaTextPattern2Caret, now - 5);
        const auto pointer = make(PoiKind::Pointer, PoiSource::Pointer, now - 10);
        const auto best = select_best_poi({caret, pointer}, now, freq);
        require(best.has_value(), "recent_pointer", "expected a POI", failures);
        if (best) require(best->source == PoiSource::Pointer, "recent_pointer", "deliberate recent pointer movement should override caret", failures);
    }

    {
        const std::uint64_t now = 20'000;
        const auto caret = make(PoiKind::Caret, PoiSource::UiaTextPattern2Caret, now - 5);
        const auto pointer = make(PoiKind::Pointer, PoiSource::Pointer, now - 150); // stale under default 100 ms pointer age
        const auto best = select_best_poi({caret, pointer}, now, freq);
        require(best.has_value(), "caret_resumes", "expected a POI", failures);
        if (best) require(best->source == PoiSource::UiaTextPattern2Caret, "caret_resumes", "caret should resume after pointer evidence ages out", failures);
    }

    {
        const std::uint64_t now = 30'000;
        const auto explicitTarget = make(PoiKind::ExplicitTarget, PoiSource::ExplicitUserTarget, now - 10);
        const auto pointer = make(PoiKind::Pointer, PoiSource::Pointer, now - 1);
        const auto best = select_best_poi({pointer, explicitTarget}, now, freq);
        require(best.has_value(), "explicit_target", "expected a POI", failures);
        if (best) require(best->source == PoiSource::ExplicitUserTarget, "explicit_target", "explicit user target must remain highest priority", failures);
    }

    if (!failures.empty()) {
        std::cerr << "poi_arbitration_tests FAILED: " << failures.size() << " failure(s)\n";
        for (const auto& f : failures) std::cerr << "  " << f.name << ": " << f.message << "\n";
        return 1;
    }
    std::cout << "poi_arbitration_tests PASSED\n";
    return 0;
}
