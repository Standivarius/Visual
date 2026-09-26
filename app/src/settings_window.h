#pragma once

#include "core/settings_model.h"

#include <windows.h>

#include <functional>
#include <string>
#include <vector>

namespace visual::ui {

struct MonitorOption {
    std::wstring device_id;
    std::wstring label;
};

class SettingsWindow {
public:
    using ApplyCallback = std::function<void(const visual::core::VisualSettings&, bool display_roles_changed)>;

    SettingsWindow() = default;
    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;
    ~SettingsWindow();

    bool create(HINSTANCE instance,
                HWND owner,
                const RECT& preferred_monitor_rect,
                const std::vector<MonitorOption>& monitors,
                const visual::core::VisualSettings& initial,
                ApplyCallback callback);
    void show();
    void hide() noexcept;
    void destroy() noexcept;
    void set_settings(const visual::core::VisualSettings& settings);

    [[nodiscard]] HWND hwnd() const noexcept { return hwnd_; }

private:
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
    LRESULT handle_message(UINT message, WPARAM w_param, LPARAM l_param);
    void create_controls();
    void layout_controls(int client_width, int client_height);
    void sync_controls();
    void populate_monitor_combo(HWND combo, bool allow_none, const std::wstring& selected);
    bool collect_settings(visual::core::VisualSettings& output);
    void apply_from_controls();
    void reset_defaults();
    void set_font_recursive(HWND root);

    HINSTANCE instance_{};
    HWND owner_{};
    HWND hwnd_{};
    HFONT font_{};
    std::vector<MonitorOption> monitors_;
    visual::core::VisualSettings settings_{};
    ApplyCallback callback_{};

    HWND zoom_combo_{};
    HWND tracking_enabled_{};
    HWND follow_pointer_{};
    HWND follow_caret_{};
    HWND follow_focus_{};
    HWND marker_pointer_{};
    HWND marker_caret_{};
    HWND marker_focus_{};
    HWND context_indicator_{};
    HWND context_shade_{};
    HWND appearance_combo_{};
    HWND context_monitor_combo_{};
    HWND detail_monitor_combo_{};
    HWND reference_monitor_combo_{};
    HWND display_note_{};
};

} // namespace visual::ui
