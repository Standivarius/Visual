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
        const std::uint64_t now = 25'000;
        const auto preexistingCaret = make(PoiKind::Caret, PoiSource::UiaTextPattern2Caret, now - 200);
        const auto stoppedPointer = make(PoiKind::Pointer, PoiSource::Pointer, now - 150); // pointer stale, but newer than caret
        const auto best = select_best_poi({preexistingCaret, stoppedPointer}, now, freq);
        require(!best.has_value(), "pre_pointer_caret_suppressed", "caret evidence from before pointer movement must not reclaim the viewport", failures);
    }

    {
        const std::uint64_t now = 27'000;
        const auto stoppedPointer = make(PoiKind::Pointer, PoiSource::Pointer, now - 150);
        const auto movedCaret = make(PoiKind::Caret, PoiSource::UiaTextPattern2Caret, now - 5);
        const auto best = select_best_poi({stoppedPointer, movedCaret}, now, freq);
        require(best.has_value(), "post_pointer_caret_resumes", "caret movement after pointer intent should resume tracking", failures);
        if (best) require(best->source == PoiSource::UiaTextPattern2Caret, "post_pointer_caret_resumes", "newer caret should own the viewport", failures);
    }
    {
        const std::uint64_t now = 30'000;
        const auto explicitTarget = make(PoiKind::ExplicitTarget, PoiSource::ExplicitUserTarget, now - 10);
        const auto pointer = make(PoiKind::Pointer, PoiSource::Pointer, now - 1);
        const auto best = select_best_poi({pointer, explicitTarget}, now, freq);
        require(best.has_value(), "explicit_target", "expected a POI", failures);
        if (best) require(best->source == PoiSource::ExplicitUserTarget, "explicit_target", "explicit user target must remain highest priority", failures);
    }

    {
        auto prior = make(PoiKind::Caret, PoiSource::UiaTextPattern2Caret, 40'000);
        prior.process_id = 10;
        prior.identity = 20;
        auto blink_sample = prior;
        blink_sample.timestamp_qpc = 40'500;
        require(same_poi_target(prior, blink_sample), "stationary_caret_same_target", "unchanged caret sample should not be new activity", failures);

        auto moved = blink_sample;
        moved.screen_rect.left += 8.0;
        moved.screen_rect.right += 8.0;
        require(!same_poi_target(prior, moved), "moved_caret_new_target", "caret geometry movement must count as new activity", failures);
    }

    {
        auto prior = make(PoiKind::Focus, PoiSource::UiaFocus, 50'000);
        prior.process_id = 11;
        prior.identity = 21;
        auto repeated = prior;
        repeated.timestamp_qpc = 50'500;
        require(same_poi_target(prior, repeated), "stationary_focus_same_target", "unchanged focus sample should not be new activity", failures);

        auto changed = repeated;
        changed.identity = 22;
        require(!same_poi_target(prior, changed), "changed_focus_new_target", "focus identity change must count as new activity", failures);
    }
    if (!failures.empty()) {
        std::cerr << "poi_arbitration_tests FAILED: " << failures.size() << " failure(s)\n";
        for (const auto& f : failures) std::cerr << "  " << f.name << ": " << f.message << "\n";
        return 1;
    }
    std::cout << "poi_arbitration_tests PASSED\n";
    return 0;
}
