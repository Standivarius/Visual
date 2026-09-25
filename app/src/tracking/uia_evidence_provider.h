#pragma once

#include "core/poi_evidence.h"

#include <windows.h>
#include <ole2.h>
#include <UIAutomation.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace visual::tracking {

class UiaEvidenceProvider {
public:
    struct Snapshot {
        std::vector<core::PoiCandidate> candidates;
        std::uint64_t sampled_qpc{};
        bool current_caret_present{};
        bool current_focus_present{};
    };

    UiaEvidenceProvider()
        : worker_([this](std::stop_token stop) { run(stop); }) {}

    UiaEvidenceProvider(const UiaEvidenceProvider&) = delete;
    UiaEvidenceProvider& operator=(const UiaEvidenceProvider&) = delete;

    [[nodiscard]] std::vector<core::PoiCandidate> snapshot() const {
        std::scoped_lock lock(mutex_);
        return latest_;
    }

    [[nodiscard]] Snapshot diagnostic_snapshot() const {
        std::scoped_lock lock(mutex_);
        return Snapshot{latest_, sampled_qpc_, current_caret_present_, current_focus_present_};
    }

private:
    static std::uint64_t qpc_now() noexcept {
        LARGE_INTEGER qpc{};
        QueryPerformanceCounter(&qpc);
        return static_cast<std::uint64_t>(qpc.QuadPart);
    }

    static bool first_bounding_rect(IUIAutomationTextRange* range, core::ScreenRect& result) noexcept {
        if (!range) return false;
        SAFEARRAY* sa = nullptr;
        if (FAILED(range->GetBoundingRectangles(&sa)) || !sa) return false;
        bool ok = false;
        if (SafeArrayGetDim(sa) == 1) {
            LONG lb = 0, ub = -1;
            if (SUCCEEDED(SafeArrayGetLBound(sa, 1, &lb)) && SUCCEEDED(SafeArrayGetUBound(sa, 1, &ub)) && ub - lb + 1 >= 4) {
                double* data = nullptr;
                if (SUCCEEDED(SafeArrayAccessData(sa, reinterpret_cast<void**>(&data))) && data) {
                    const double left = data[0];
                    const double top = data[1];
                    const double width = data[2];
                    const double height = data[3];
                    result = {left, top, left + width, top + height};
                    ok = result.valid();
                    SafeArrayUnaccessData(sa);
                }
            }
        }
        SafeArrayDestroy(sa);
        return ok;
    }

    static bool character_context_rect(IUIAutomationTextRange* range, core::ScreenRect& result) noexcept {
        if (!range) return false;
        IUIAutomationTextRange* expanded = nullptr;
        if (FAILED(range->Clone(&expanded)) || !expanded) return false;
        const HRESULT hr = expanded->ExpandToEnclosingUnit(TextUnit_Character);
        const bool ok = SUCCEEDED(hr) && first_bounding_rect(expanded, result);
        expanded->Release();
        return ok;
    }

    static bool range_is_degenerate(IUIAutomationTextRange* range) noexcept {
        if (!range) return false;
        int delta = 0;
        if (FAILED(range->CompareEndpoints(TextPatternRangeEndpoint_Start, range, TextPatternRangeEndpoint_End, &delta))) return false;
        return delta == 0;
    }

    static IUIAutomationTextRange* first_range(IUIAutomationTextRangeArray* ranges) noexcept {
        if (!ranges) return nullptr;
        int length = 0;
        if (FAILED(ranges->get_Length(&length)) || length <= 0) return nullptr;
        IUIAutomationTextRange* range = nullptr;
        if (FAILED(ranges->GetElement(0, &range))) return nullptr;
        return range;
    }

    static void sample_once(IUIAutomation* uia, std::vector<core::PoiCandidate>& out) noexcept {
        IUIAutomationElement* focused = nullptr;
        if (FAILED(uia->GetFocusedElement(&focused)) || !focused) return;

        const auto now = qpc_now();
        int process_id = 0;
        focused->get_CurrentProcessId(&process_id);

        RECT focus_rect{};
        if (SUCCEEDED(focused->get_CurrentBoundingRectangle(&focus_rect))) {
            core::PoiCandidate focus{};
            focus.kind = core::PoiKind::Focus;
            focus.source = core::PoiSource::UiaFocus;
            focus.confidence = core::PoiConfidence::High;
            focus.screen_rect = {
                static_cast<double>(focus_rect.left), static_cast<double>(focus_rect.top),
                static_cast<double>(focus_rect.right), static_cast<double>(focus_rect.bottom)
            };
            focus.timestamp_qpc = now;
            focus.process_id = static_cast<std::uint32_t>(std::max(0, process_id));
            if (focus.usable()) out.push_back(focus);
        }

        bool have_semantic_caret = false;
        IUIAutomationTextPattern2* text2 = nullptr;
        if (SUCCEEDED(focused->GetCurrentPatternAs(
                UIA_TextPattern2Id,
                __uuidof(IUIAutomationTextPattern2),
                reinterpret_cast<void**>(&text2))) && text2) {
            BOOL active = FALSE;
            IUIAutomationTextRange* caret_range = nullptr;
            if (SUCCEEDED(text2->GetCaretRange(&active, &caret_range)) && caret_range) {
                core::ScreenRect rect{};
                if (!first_bounding_rect(caret_range, rect)) character_context_rect(caret_range, rect);
                if (rect.valid()) {
                    core::PoiCandidate caret{};
                    caret.kind = core::PoiKind::Caret;
                    caret.source = core::PoiSource::UiaTextPattern2Caret;
                    caret.confidence = active ? core::PoiConfidence::High : core::PoiConfidence::Medium;
                    caret.screen_rect = rect;
                    caret.timestamp_qpc = now;
                    caret.process_id = static_cast<std::uint32_t>(std::max(0, process_id));
                    out.push_back(caret);
                    have_semantic_caret = true;
                }
                caret_range->Release();
            }
            text2->Release();
        }

        if (!have_semantic_caret) {
            IUIAutomationTextPattern* text = nullptr;
            if (SUCCEEDED(focused->GetCurrentPatternAs(
                    UIA_TextPatternId,
                    __uuidof(IUIAutomationTextPattern),
                    reinterpret_cast<void**>(&text))) && text) {
                IUIAutomationTextRangeArray* selections = nullptr;
                if (SUCCEEDED(text->GetSelection(&selections)) && selections) {
                    IUIAutomationTextRange* range = first_range(selections);
                    if (range) {
                        if (range_is_degenerate(range)) {
                            core::ScreenRect rect{};
                            if (!first_bounding_rect(range, rect)) character_context_rect(range, rect);
                            if (rect.valid()) {
                                core::PoiCandidate caret{};
                                caret.kind = core::PoiKind::Caret;
                                caret.source = core::PoiSource::UiaTextPatternSelection;
                                caret.confidence = core::PoiConfidence::High;
                                caret.screen_rect = rect;
                                caret.timestamp_qpc = now;
                                caret.process_id = static_cast<std::uint32_t>(std::max(0, process_id));
                                out.push_back(caret);
                            }
                        }
                        range->Release();
                    }
                    selections->Release();
                }
                text->Release();
            }
        }

        focused->Release();
    }

    static bool is_semantic_caret(const core::PoiCandidate& candidate) noexcept {
        return candidate.source == core::PoiSource::UiaTextPattern2Caret
            || candidate.source == core::PoiSource::UiaTextPatternSelection;
    }

    void run(std::stop_token stop) noexcept {
        const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(co) && co != RPC_E_CHANGED_MODE) return;

        IUIAutomation* uia = nullptr;
        const HRESULT create = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&uia));
        if (SUCCEEDED(create) && uia) {
            while (!stop.stop_requested()) {
                std::vector<core::PoiCandidate> sampled;
                sample_once(uia, sampled);
                const auto sampled_qpc = qpc_now();

                bool current_caret = false;
                bool current_focus = false;
                std::optional<std::size_t> semantic_caret_index;
                std::optional<std::size_t> focus_index;
                for (std::size_t i = 0; i < sampled.size(); ++i) {
                    if (is_semantic_caret(sampled[i])) {
                        current_caret = true;
                        semantic_caret_index = i;
                    }
                    if (sampled[i].source == core::PoiSource::UiaFocus) {
                        current_focus = true;
                        focus_index = i;
                    }
                }

                {
                    std::scoped_lock lock(mutex_);
                    if (focus_index) {
                        auto& focus = sampled[*focus_index];
                        if (last_focus_ && core::same_poi_target(focus, *last_focus_)) focus.timestamp_qpc = last_focus_->timestamp_qpc;
                        last_focus_ = focus;
                    }
                    if (semantic_caret_index) {
                        auto& caret = sampled[*semantic_caret_index];
                        if (last_semantic_caret_ && core::same_poi_target(caret, *last_semantic_caret_)) caret.timestamp_qpc = last_semantic_caret_->timestamp_qpc;
                        last_semantic_caret_ = caret;
                    } else if (last_semantic_caret_) {
                        // Retain the last real semantic caret with its ORIGINAL timestamp. The
                        // core freshness policy therefore expires it naturally instead of a
                        // transient UIA miss causing an immediate focus-only handoff.
                        sampled.push_back(*last_semantic_caret_);
                    }
                    latest_ = std::move(sampled);
                    sampled_qpc_ = sampled_qpc;
                    current_caret_present_ = current_caret;
                    current_focus_present_ = current_focus;
                }

                for (int i = 0; i < 3 && !stop.stop_requested(); ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
            uia->Release();
        }

        if (SUCCEEDED(co)) CoUninitialize();
    }

    mutable std::mutex mutex_;
    std::vector<core::PoiCandidate> latest_;
    std::optional<core::PoiCandidate> last_semantic_caret_;
    std::optional<core::PoiCandidate> last_focus_;
    std::uint64_t sampled_qpc_{};
    bool current_caret_present_{};
    bool current_focus_present_{};
    std::jthread worker_;
};

} // namespace visual::tracking
