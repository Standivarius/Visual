#include "settings_window.h"

#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace visual::ui {
namespace {

constexpr wchar_t kSettingsClass[] = L"VisualSettingsWindow";
constexpr int kWindowWidth = 720;
constexpr int kWindowHeight = 760;

constexpr int kIdZoom = 1001;
constexpr int kIdTrackingEnabled = 1002;
constexpr int kIdFollowPointer = 1003;
constexpr int kIdFollowCaret = 1004;
constexpr int kIdFollowFocus = 1005;
constexpr int kIdMarkerPointer = 1006;
constexpr int kIdMarkerCaret = 1007;
constexpr int kIdMarkerFocus = 1008;
constexpr int kIdContextIndicator = 1009;
constexpr int kIdContextShade = 1010;
constexpr int kIdAppearance = 1011;
constexpr int kIdContextMonitor = 1012;
constexpr int kIdDetailMonitor = 1013;
constexpr int kIdReferenceMonitor = 1014;
constexpr int kIdApply = 1020;
constexpr int kIdDefaults = 1021;
constexpr int kIdClose = 1022;

HWND make_control(HWND parent, const wchar_t* klass, const wchar_t* text, DWORD style, int id) {
    return CreateWindowExW(0, klass, text, WS_CHILD | WS_VISIBLE | style,
                           0, 0, 10, 10, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                           GetModuleHandleW(nullptr), nullptr);
}

void set_check(HWND hwnd, bool checked) {
    Button_SetCheck(hwnd, checked ? BST_CHECKED : BST_UNCHECKED);
}

bool is_checked(HWND hwnd) {
    return Button_GetCheck(hwnd) == BST_CHECKED;
}

int combo_selected(HWND combo) {
    return static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
}

void combo_select(HWND combo, int index) {
    SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
}

} // namespace

SettingsWindow::~SettingsWindow() {
    destroy();
}

bool SettingsWindow::create(HINSTANCE instance,
                            HWND owner,
                            const RECT& preferred_monitor_rect,
                            const std::vector<MonitorOption>& monitors,
                            const visual::core::VisualSettings& initial,
                            ApplyCallback callback) {
    destroy();
    instance_ = instance;
    owner_ = owner;
    monitors_ = monitors;
    settings_ = initial;
    callback_ = std::move(callback);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = &SettingsWindow::window_proc;
    wc.lpszClassName = kSettingsClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    const int available_width = preferred_monitor_rect.right - preferred_monitor_rect.left;
    const int available_height = preferred_monitor_rect.bottom - preferred_monitor_rect.top;
    const int width = std::min(kWindowWidth, std::max(520, available_width - 80));
    const int height = std::min(kWindowHeight, std::max(620, available_height - 80));
    const int x = preferred_monitor_rect.left + std::max(20, (available_width - width) / 2);
    const int y = preferred_monitor_rect.top + std::max(20, (available_height - height) / 2);

    hwnd_ = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_CONTROLPARENT,
                            kSettingsClass,
                            L"Visual Settings",
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
                            x, y, width, height, owner, nullptr, instance, this);
    return hwnd_ != nullptr;
}

void SettingsWindow::show() {
    if (!hwnd_) return;
    sync_controls();
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd_);
}

void SettingsWindow::hide() noexcept {
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

void SettingsWindow::destroy() noexcept {
    if (hwnd_) DestroyWindow(hwnd_);
    hwnd_ = nullptr;
    if (font_) DeleteObject(font_);
    font_ = nullptr;
    callback_ = {};
    monitors_.clear();
}

void SettingsWindow::set_settings(const visual::core::VisualSettings& settings) {
    settings_ = settings;
    if (hwnd_ && IsWindowVisible(hwnd_)) sync_controls();
}

LRESULT CALLBACK SettingsWindow::window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    SettingsWindow* self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
        self = static_cast<SettingsWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->hwnd_ = hwnd;
    }
    return self ? self->handle_message(message, w_param, l_param)
                : DefWindowProcW(hwnd, message, w_param, l_param);
}

LRESULT SettingsWindow::handle_message(UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
    case WM_CREATE:
        create_controls();
        return 0;
    case WM_SIZE:
        layout_controls(LOWORD(l_param), HIWORD(l_param));
        return 0;
    case WM_COMMAND:
        switch (LOWORD(w_param)) {
        case kIdApply:
            apply_from_controls();
            return 0;
        case kIdDefaults:
            reset_defaults();
            return 0;
        case kIdClose:
            hide();
            return 0;
        case kIdContextIndicator:
            EnableWindow(context_shade_, is_checked(context_indicator_));
            return 0;
        default:
            break;
        }
        break;
    case WM_CLOSE:
        hide();
        return 0;
    case WM_DESTROY:
        hwnd_ = nullptr;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd_, message, w_param, l_param);
}

void SettingsWindow::create_controls() {
    font_ = CreateFontW(-19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    make_control(hwnd_, L"STATIC", L"Magnification", SS_LEFT, -1);
    zoom_combo_ = make_control(hwnd_, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, kIdZoom);
    SendMessageW(zoom_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1x — normal view"));
    SendMessageW(zoom_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1.5x"));
    SendMessageW(zoom_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"2x"));
    SendMessageW(zoom_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"3x"));
    SendMessageW(zoom_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"4x"));

    make_control(hwnd_, L"BUTTON", L"Follow what I am using", BS_GROUPBOX, -1);
    tracking_enabled_ = make_control(hwnd_, L"BUTTON", L"&Follow activity", BS_AUTOCHECKBOX | WS_TABSTOP, kIdTrackingEnabled);
    follow_pointer_ = make_control(hwnd_, L"BUTTON", L"Follow &pointer", BS_AUTOCHECKBOX | WS_TABSTOP, kIdFollowPointer);
    follow_caret_ = make_control(hwnd_, L"BUTTON", L"Follow text &caret", BS_AUTOCHECKBOX | WS_TABSTOP, kIdFollowCaret);
    follow_focus_ = make_control(hwnd_, L"BUTTON", L"Follow keyboard &focus", BS_AUTOCHECKBOX | WS_TABSTOP, kIdFollowFocus);

    make_control(hwnd_, L"BUTTON", L"High-visibility markers", BS_GROUPBOX, -1);
    marker_pointer_ = make_control(hwnd_, L"BUTTON", L"Show pointer marker", BS_AUTOCHECKBOX | WS_TABSTOP, kIdMarkerPointer);
    marker_caret_ = make_control(hwnd_, L"BUTTON", L"Show caret marker", BS_AUTOCHECKBOX | WS_TABSTOP, kIdMarkerCaret);
    marker_focus_ = make_control(hwnd_, L"BUTTON", L"Show focus marker", BS_AUTOCHECKBOX | WS_TABSTOP, kIdMarkerFocus);

    make_control(hwnd_, L"BUTTON", L"Context screen", BS_GROUPBOX, -1);
    context_indicator_ = make_control(hwnd_, L"BUTTON", L"Show the &Detail View rectangle", BS_AUTOCHECKBOX | WS_TABSTOP, kIdContextIndicator);
    context_shade_ = make_control(hwnd_, L"BUTTON", L"Lightly shade the Detail View area", BS_AUTOCHECKBOX | WS_TABSTOP, kIdContextShade);

    make_control(hwnd_, L"STATIC", L"Appearance", SS_LEFT, -1);
    appearance_combo_ = make_control(hwnd_, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, kIdAppearance);
    SendMessageW(appearance_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Normal"));
    SendMessageW(appearance_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"High contrast"));
    SendMessageW(appearance_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Inverted colours"));
    SendMessageW(appearance_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Grayscale"));

    make_control(hwnd_, L"BUTTON", L"Screen roles", BS_GROUPBOX, -1);
    make_control(hwnd_, L"STATIC", L"Context", SS_LEFT, -1);
    context_monitor_combo_ = make_control(hwnd_, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, kIdContextMonitor);
    make_control(hwnd_, L"STATIC", L"Detail", SS_LEFT, -1);
    detail_monitor_combo_ = make_control(hwnd_, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, kIdDetailMonitor);
    make_control(hwnd_, L"STATIC", L"Reference", SS_LEFT, -1);
    reference_monitor_combo_ = make_control(hwnd_, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, kIdReferenceMonitor);
    display_note_ = make_control(hwnd_, L"STATIC", L"Screen-role changes are saved and take effect the next time Visual starts. The Reference screen is left unchanged for normal Windows use.", SS_LEFT, -1);

    make_control(hwnd_, L"BUTTON", L"&Apply", BS_DEFPUSHBUTTON | WS_TABSTOP, kIdApply);
    make_control(hwnd_, L"BUTTON", L"Restore &defaults", BS_PUSHBUTTON | WS_TABSTOP, kIdDefaults);
    make_control(hwnd_, L"BUTTON", L"&Close", BS_PUSHBUTTON | WS_TABSTOP, kIdClose);

    set_font_recursive(hwnd_);
    sync_controls();
}

void SettingsWindow::layout_controls(int client_width, int client_height) {
    if (!zoom_combo_) return;
    const int margin = 24;
    const int content_width = std::max(440, client_width - margin * 2);
    const int label_width = 135;
    const int combo_x = margin + label_width;
    const int combo_width = content_width - label_width;
    int y = 22;

    auto next_static = [&](const wchar_t* text) -> HWND {
        HWND child = FindWindowExW(hwnd_, nullptr, L"STATIC", text);
        return child;
    };
    auto next_group = [&](const wchar_t* text) -> HWND {
        HWND child = FindWindowExW(hwnd_, nullptr, L"BUTTON", text);
        return child;
    };

    SetWindowPos(next_static(L"Magnification"), nullptr, margin, y + 4, label_width - 12, 30, SWP_NOZORDER);
    SetWindowPos(zoom_combo_, nullptr, combo_x, y, combo_width, 220, SWP_NOZORDER);
    y += 54;

    SetWindowPos(next_group(L"Follow what I am using"), nullptr, margin, y, content_width, 132, SWP_NOZORDER);
    SetWindowPos(tracking_enabled_, nullptr, margin + 18, y + 28, content_width - 36, 26, SWP_NOZORDER);
    SetWindowPos(follow_pointer_, nullptr, margin + 38, y + 58, 170, 26, SWP_NOZORDER);
    SetWindowPos(follow_caret_, nullptr, margin + 220, y + 58, 185, 26, SWP_NOZORDER);
    SetWindowPos(follow_focus_, nullptr, margin + 410, y + 58, std::max(170, content_width - 430), 26, SWP_NOZORDER);
    y += 144;

    SetWindowPos(next_group(L"High-visibility markers"), nullptr, margin, y, content_width, 86, SWP_NOZORDER);
    SetWindowPos(marker_pointer_, nullptr, margin + 18, y + 32, 185, 26, SWP_NOZORDER);
    SetWindowPos(marker_caret_, nullptr, margin + 215, y + 32, 175, 26, SWP_NOZORDER);
    SetWindowPos(marker_focus_, nullptr, margin + 400, y + 32, std::max(175, content_width - 420), 26, SWP_NOZORDER);
    y += 98;

    SetWindowPos(next_group(L"Context screen"), nullptr, margin, y, content_width, 92, SWP_NOZORDER);
    SetWindowPos(context_indicator_, nullptr, margin + 18, y + 28, content_width - 36, 26, SWP_NOZORDER);
    SetWindowPos(context_shade_, nullptr, margin + 38, y + 56, content_width - 56, 26, SWP_NOZORDER);
    y += 104;

    SetWindowPos(next_static(L"Appearance"), nullptr, margin, y + 4, label_width - 12, 30, SWP_NOZORDER);
    SetWindowPos(appearance_combo_, nullptr, combo_x, y, combo_width, 220, SWP_NOZORDER);
    y += 54;

    const int screen_group_height = std::max(190, client_height - y - 96);
    SetWindowPos(next_group(L"Screen roles"), nullptr, margin, y, content_width, screen_group_height, SWP_NOZORDER);
    const int row_x = margin + 18;
    const int row_label = 110;
    const int screen_combo_x = row_x + row_label;
    const int screen_combo_width = content_width - 36 - row_label;
    HWND context_label = FindWindowExW(hwnd_, nullptr, L"STATIC", L"Context");
    HWND detail_label = FindWindowExW(hwnd_, nullptr, L"STATIC", L"Detail");
    HWND reference_label = FindWindowExW(hwnd_, nullptr, L"STATIC", L"Reference");
    SetWindowPos(context_label, nullptr, row_x, y + 32, row_label - 8, 26, SWP_NOZORDER);
    SetWindowPos(context_monitor_combo_, nullptr, screen_combo_x, y + 28, screen_combo_width, 220, SWP_NOZORDER);
    SetWindowPos(detail_label, nullptr, row_x, y + 70, row_label - 8, 26, SWP_NOZORDER);
    SetWindowPos(detail_monitor_combo_, nullptr, screen_combo_x, y + 66, screen_combo_width, 220, SWP_NOZORDER);
    SetWindowPos(reference_label, nullptr, row_x, y + 108, row_label - 8, 26, SWP_NOZORDER);
    SetWindowPos(reference_monitor_combo_, nullptr, screen_combo_x, y + 104, screen_combo_width, 220, SWP_NOZORDER);
    SetWindowPos(display_note_, nullptr, row_x, y + 142, content_width - 36, std::max(42, screen_group_height - 150), SWP_NOZORDER);

    const int button_y = client_height - 54;
    const int button_width = 150;
    SetWindowPos(GetDlgItem(hwnd_, kIdApply), nullptr, client_width - margin - button_width, button_y, button_width, 36, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd_, kIdClose), nullptr, client_width - margin - button_width * 2 - 12, button_y, button_width, 36, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd_, kIdDefaults), nullptr, margin, button_y, 175, 36, SWP_NOZORDER);
}

void SettingsWindow::sync_controls() {
    if (!zoom_combo_) return;
    int zoom_index = 2;
    for (std::size_t i = 0; i < visual::core::kSupportedZoomLevels.size(); ++i) {
        if (visual::core::near_zoom(settings_.zoom, visual::core::kSupportedZoomLevels[i])) zoom_index = static_cast<int>(i);
    }
    combo_select(zoom_combo_, zoom_index);
    set_check(tracking_enabled_, settings_.tracking_enabled);
    set_check(follow_pointer_, settings_.follow_pointer);
    set_check(follow_caret_, settings_.follow_caret);
    set_check(follow_focus_, settings_.follow_focus);
    set_check(marker_pointer_, settings_.show_pointer_locator);
    set_check(marker_caret_, settings_.show_caret_locator);
    set_check(marker_focus_, settings_.show_focus_locator);
    set_check(context_indicator_, settings_.show_context_indicator);
    set_check(context_shade_, settings_.shade_context_indicator);
    EnableWindow(context_shade_, settings_.show_context_indicator);
    combo_select(appearance_combo_, static_cast<int>(settings_.visual_mode));
    populate_monitor_combo(context_monitor_combo_, false, settings_.context_monitor_device);
    populate_monitor_combo(detail_monitor_combo_, false, settings_.detail_monitor_device);
    populate_monitor_combo(reference_monitor_combo_, true, settings_.reference_monitor_device);
    const bool multi_monitor_roles = monitors_.size() >= 2;
    EnableWindow(context_monitor_combo_, multi_monitor_roles);
    EnableWindow(detail_monitor_combo_, multi_monitor_roles);
    EnableWindow(reference_monitor_combo_, multi_monitor_roles);
    if (!multi_monitor_roles) {
        SetWindowTextW(display_note_, L"Screen roles require at least two active screens. Visual is currently using its single-screen fallback.");
    } else {
        SetWindowTextW(display_note_, L"Screen-role changes are saved and take effect the next time Visual starts. The Reference screen is left unchanged for normal Windows use.");
    }
}

void SettingsWindow::populate_monitor_combo(HWND combo, bool allow_none, const std::wstring& selected) {
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    int selected_index = -1;
    int index = 0;
    if (allow_none) {
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"None / leave unassigned"));
        if (selected.empty()) selected_index = 0;
        ++index;
    }
    for (const auto& monitor : monitors_) {
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(monitor.label.c_str()));
        if (monitor.device_id == selected) selected_index = index;
        ++index;
    }
    if (selected_index < 0 && !monitors_.empty()) selected_index = allow_none ? 0 : 0;
    combo_select(combo, selected_index);
}

bool SettingsWindow::collect_settings(visual::core::VisualSettings& output) {
    output = settings_;
    const int zoom_index = combo_selected(zoom_combo_);
    if (zoom_index < 0 || zoom_index >= static_cast<int>(visual::core::kSupportedZoomLevels.size())) return false;
    output.zoom = visual::core::kSupportedZoomLevels[static_cast<std::size_t>(zoom_index)];
    output.tracking_enabled = is_checked(tracking_enabled_);
    output.follow_pointer = is_checked(follow_pointer_);
    output.follow_caret = is_checked(follow_caret_);
    output.follow_focus = is_checked(follow_focus_);
    output.show_pointer_locator = is_checked(marker_pointer_);
    output.show_caret_locator = is_checked(marker_caret_);
    output.show_focus_locator = is_checked(marker_focus_);
    output.show_context_indicator = is_checked(context_indicator_);
    output.shade_context_indicator = is_checked(context_shade_);
    output.visual_mode = visual::core::sanitize_visual_mode(combo_selected(appearance_combo_));

    const auto monitor_from_combo = [&](HWND combo, bool allow_none) -> std::wstring {
        const int raw = combo_selected(combo);
        if (raw < 0) return {};
        if (allow_none && raw == 0) return {};
        const int monitor_index = raw - (allow_none ? 1 : 0);
        if (monitor_index < 0 || monitor_index >= static_cast<int>(monitors_.size())) return {};
        return monitors_[static_cast<std::size_t>(monitor_index)].device_id;
    };
    if (monitors_.size() < 2) {
        output.context_monitor_device.clear();
        output.detail_monitor_device.clear();
        output.reference_monitor_device.clear();
        return true;
    }
    output.context_monitor_device = monitor_from_combo(context_monitor_combo_, false);
    output.detail_monitor_device = monitor_from_combo(detail_monitor_combo_, false);
    output.reference_monitor_device = monitor_from_combo(reference_monitor_combo_, true);

    if (!visual::core::monitor_roles_valid(output)) {
        MessageBoxW(hwnd_, L"Context, Detail and Reference must use different screens.", L"Visual Settings", MB_OK | MB_ICONWARNING);
        return false;
    }
    return true;
}

void SettingsWindow::apply_from_controls() {
    visual::core::VisualSettings next{};
    if (!collect_settings(next)) return;
    const bool roles_changed = next.context_monitor_device != settings_.context_monitor_device
        || next.detail_monitor_device != settings_.detail_monitor_device
        || next.reference_monitor_device != settings_.reference_monitor_device;
    settings_ = next;
    if (callback_) callback_(settings_, roles_changed);
}

void SettingsWindow::reset_defaults() {
    visual::core::VisualSettings defaults{};
    defaults.context_monitor_device = settings_.context_monitor_device;
    defaults.detail_monitor_device = settings_.detail_monitor_device;
    defaults.reference_monitor_device = settings_.reference_monitor_device;
    settings_ = defaults;
    sync_controls();
}

void SettingsWindow::set_font_recursive(HWND root) {
    if (!font_) return;
    SendMessageW(root, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    for (HWND child = GetWindow(root, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
}

} // namespace visual::ui
