#include "settings_store.h"

#include <windows.h>
#include <shlobj.h>

#include <array>
#include <cwchar>
#include <string>
#include <stdexcept>

namespace visual::settings {
namespace {

std::wstring read_string(const std::filesystem::path& path, const wchar_t* section, const wchar_t* key, const wchar_t* fallback = L"") {
    std::array<wchar_t, 1024> buffer{};
    GetPrivateProfileStringW(section, key, fallback, buffer.data(), static_cast<DWORD>(buffer.size()), path.c_str());
    return buffer.data();
}

bool read_bool(const std::filesystem::path& path, const wchar_t* section, const wchar_t* key, bool fallback) {
    return GetPrivateProfileIntW(section, key, fallback ? 1 : 0, path.c_str()) != 0;
}

double read_double(const std::filesystem::path& path, const wchar_t* section, const wchar_t* key, double fallback) {
    wchar_t fallbackBuffer[32]{};
    swprintf_s(fallbackBuffer, L"%.3f", fallback);
    const auto text = read_string(path, section, key, fallbackBuffer);
    wchar_t* end = nullptr;
    const double value = std::wcstod(text.c_str(), &end);
    return end && end != text.c_str() ? value : fallback;
}

bool write_string(const std::filesystem::path& path, const wchar_t* section, const wchar_t* key, const std::wstring& value) noexcept {
    return WritePrivateProfileStringW(section, key, value.c_str(), path.c_str()) != FALSE;
}

bool write_bool(const std::filesystem::path& path, const wchar_t* section, const wchar_t* key, bool value) noexcept {
    return WritePrivateProfileStringW(section, key, value ? L"1" : L"0", path.c_str()) != FALSE;
}

bool write_int(const std::filesystem::path& path, const wchar_t* section, const wchar_t* key, int value) noexcept {
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%d", value);
    return WritePrivateProfileStringW(section, key, buffer, path.c_str()) != FALSE;
}

bool write_double(const std::filesystem::path& path, const wchar_t* section, const wchar_t* key, double value) noexcept {
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%.3f", value);
    return WritePrivateProfileStringW(section, key, buffer, path.c_str()) != FALSE;
}

} // namespace

std::filesystem::path default_settings_path() {
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &raw)) || !raw) {
        throw std::runtime_error("Unable to resolve LocalAppData for Visual settings");
    }
    std::filesystem::path root(raw);
    CoTaskMemFree(raw);
    root /= L"Standivarius";
    root /= L"Visual";
    std::filesystem::create_directories(root);
    return root / L"settings.ini";
}

visual::core::VisualSettings load_settings(const std::filesystem::path& path) {
    visual::core::VisualSettings settings{};
    if (!std::filesystem::exists(path)) return settings;

    settings.zoom = visual::core::sanitize_zoom(read_double(path, L"Magnification", L"Zoom", settings.zoom));
    settings.tracking_enabled = read_bool(path, L"Tracking", L"Enabled", settings.tracking_enabled);
    settings.follow_pointer = read_bool(path, L"Tracking", L"FollowPointer", settings.follow_pointer);
    settings.follow_caret = read_bool(path, L"Tracking", L"FollowCaret", settings.follow_caret);
    settings.follow_focus = read_bool(path, L"Tracking", L"FollowFocus", settings.follow_focus);
    settings.show_pointer_locator = read_bool(path, L"Visibility", L"PointerMarker", settings.show_pointer_locator);
    settings.show_caret_locator = read_bool(path, L"Visibility", L"CaretMarker", settings.show_caret_locator);
    settings.show_focus_locator = read_bool(path, L"Visibility", L"FocusMarker", settings.show_focus_locator);
    settings.show_context_indicator = read_bool(path, L"Context", L"ShowDetailView", settings.show_context_indicator);
    settings.shade_context_indicator = read_bool(path, L"Context", L"ShadeDetailView", settings.shade_context_indicator);
    settings.visual_mode = visual::core::sanitize_visual_mode(GetPrivateProfileIntW(
        L"Appearance", L"Mode", static_cast<int>(settings.visual_mode), path.c_str()));
    settings.context_monitor_device = read_string(path, L"Displays", L"Context");
    settings.detail_monitor_device = read_string(path, L"Displays", L"Detail");
    settings.reference_monitor_device = read_string(path, L"Displays", L"Reference");

    if (!visual::core::monitor_roles_valid(settings)) {
        settings.context_monitor_device.clear();
        settings.detail_monitor_device.clear();
        settings.reference_monitor_device.clear();
    }
    return settings;
}

bool save_settings(const std::filesystem::path& path, const visual::core::VisualSettings& input) noexcept {
    try {
        auto settings = input;
        settings.zoom = visual::core::sanitize_zoom(settings.zoom);
        if (!visual::core::monitor_roles_valid(settings)) return false;
        std::filesystem::create_directories(path.parent_path());

        bool ok = true;
        ok = write_double(path, L"Magnification", L"Zoom", settings.zoom) && ok;
        ok = write_bool(path, L"Tracking", L"Enabled", settings.tracking_enabled) && ok;
        ok = write_bool(path, L"Tracking", L"FollowPointer", settings.follow_pointer) && ok;
        ok = write_bool(path, L"Tracking", L"FollowCaret", settings.follow_caret) && ok;
        ok = write_bool(path, L"Tracking", L"FollowFocus", settings.follow_focus) && ok;
        ok = write_bool(path, L"Visibility", L"PointerMarker", settings.show_pointer_locator) && ok;
        ok = write_bool(path, L"Visibility", L"CaretMarker", settings.show_caret_locator) && ok;
        ok = write_bool(path, L"Visibility", L"FocusMarker", settings.show_focus_locator) && ok;
        ok = write_bool(path, L"Context", L"ShowDetailView", settings.show_context_indicator) && ok;
        ok = write_bool(path, L"Context", L"ShadeDetailView", settings.shade_context_indicator) && ok;
        ok = write_int(path, L"Appearance", L"Mode", static_cast<int>(settings.visual_mode)) && ok;
        ok = write_string(path, L"Displays", L"Context", settings.context_monitor_device) && ok;
        ok = write_string(path, L"Displays", L"Detail", settings.detail_monitor_device) && ok;
        ok = write_string(path, L"Displays", L"Reference", settings.reference_monitor_device) && ok;
        WritePrivateProfileStringW(nullptr, nullptr, nullptr, path.c_str());
        return ok;
    } catch (...) {
        return false;
    }
}

} // namespace visual::settings
