#include <windows.h>
#include <shellscalingapi.h>
#include <dxgi1_2.h>
#include <winternl.h>

#include "visual_version.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct MonitorDiagnostic {
    std::wstring deviceName;
    RECT bounds{};
    bool primary{};
    DWORD width{};
    DWORD height{};
    DWORD refreshHz{};
    UINT dpiX{96};
    UINT dpiY{96};
};

std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string result(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), needed, nullptr, nullptr);
    return result;
}

std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (unsigned char ch : value) {
        switch (ch) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (ch < 0x20) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch)
                    << std::dec << std::setfill(' ');
            } else {
                out << static_cast<char>(ch);
            }
        }
    }
    return out.str();
}

std::string quoted(const std::string& value) {
    return std::string("\"") + json_escape(value) + "\"";
}

std::filesystem::path module_path() {
    std::wstring buffer(32768, L'\0');
    const DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (len == 0 || len >= buffer.size()) return {};
    buffer.resize(len);
    return std::filesystem::path(buffer);
}

std::string utc_now_iso8601() {
    SYSTEMTIME st{};
    GetSystemTime(&st);
    std::ostringstream out;
    out << std::setfill('0')
        << std::setw(4) << st.wYear << '-'
        << std::setw(2) << st.wMonth << '-'
        << std::setw(2) << st.wDay << 'T'
        << std::setw(2) << st.wHour << ':'
        << std::setw(2) << st.wMinute << ':'
        << std::setw(2) << st.wSecond << '.'
        << std::setw(3) << st.wMilliseconds << 'Z';
    return out.str();
}

std::string process_architecture() {
#if defined(_M_X64)
    return "x64";
#elif defined(_M_ARM64)
    return "arm64";
#elif defined(_M_IX86)
    return "x86";
#else
    return "unknown";
#endif
}

RTL_OSVERSIONINFOW windows_version() {
    RTL_OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof(info);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return info;
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    const auto fn = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (fn) fn(&info);
    return info;
}

BOOL CALLBACK enum_monitor(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    auto* records = reinterpret_cast<std::vector<MonitorDiagnostic>*>(data);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) return TRUE;

    MonitorDiagnostic record{};
    record.deviceName = info.szDevice;
    record.bounds = info.rcMonitor;
    record.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;

    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (EnumDisplaySettingsExW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode, 0)) {
        record.width = mode.dmPelsWidth;
        record.height = mode.dmPelsHeight;
        record.refreshHz = mode.dmDisplayFrequency;
    }

    UINT dpiX = 96, dpiY = 96;
    if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        record.dpiX = dpiX;
        record.dpiY = dpiY;
    }
    records->push_back(std::move(record));
    return TRUE;
}

std::vector<MonitorDiagnostic> monitors() {
    std::vector<MonitorDiagnostic> result;
    EnumDisplayMonitors(nullptr, nullptr, enum_monitor, reinterpret_cast<LPARAM>(&result));
    return result;
}

std::vector<std::wstring> gpu_adapters() {
    std::vector<std::wstring> result;
    IDXGIFactory1* rawFactory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&rawFactory))) || !rawFactory) return result;
    for (UINT index = 0;; ++index) {
        IDXGIAdapter1* adapter = nullptr;
        const HRESULT hr = rawFactory->EnumAdapters1(index, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(hr) || !adapter) continue;
        DXGI_ADAPTER_DESC1 desc{};
        if (SUCCEEDED(adapter->GetDesc1(&desc))) result.emplace_back(desc.Description);
        adapter->Release();
    }
    rawFactory->Release();
    return result;
}

std::string build_json() {
    const auto os = windows_version();
    const auto displayRecords = monitors();
    const auto adapters = gpu_adapters();
    const auto self = module_path();
    const auto visualApp = self.empty() ? std::filesystem::path{} : self.parent_path() / L"visual_app.exe";
    const bool visualPresent = !visualApp.empty() && std::filesystem::exists(visualApp);
    std::uintmax_t visualSize = 0;
    if (visualPresent) {
        std::error_code ec;
        visualSize = std::filesystem::file_size(visualApp, ec);
        if (ec) visualSize = 0;
    }

    std::ostringstream out;
    out << "{\n";
    out << "  \"schema_version\": \"1\",\n";
    out << "  \"app_id\": " << quoted(visual::version::kPackageId) << ",\n";
    out << "  \"visual_version\": " << quoted(visual::version::kSemanticVersion) << ",\n";
    out << "  \"release_channel\": " << quoted(visual::version::kReleaseChannel) << ",\n";
    out << "  \"generated_at_utc\": " << quoted(utc_now_iso8601()) << ",\n";
    out << "  \"process_architecture\": " << quoted(process_architecture()) << ",\n";
    out << "  \"windows\": {\"major\": " << os.dwMajorVersion
        << ", \"minor\": " << os.dwMinorVersion
        << ", \"build\": " << os.dwBuildNumber << "},\n";
    out << "  \"visual_app\": {\"present\": " << (visualPresent ? "true" : "false")
        << ", \"file_name\": \"visual_app.exe\""
        << ", \"file_size_bytes\": " << visualSize << "},\n";

    out << "  \"monitors\": [\n";
    for (std::size_t i = 0; i < displayRecords.size(); ++i) {
        const auto& m = displayRecords[i];
        out << "    {\"device_name\": " << quoted(utf8(m.deviceName))
            << ", \"primary\": " << (m.primary ? "true" : "false")
            << ", \"bounds\": {\"left\": " << m.bounds.left << ", \"top\": " << m.bounds.top
            << ", \"right\": " << m.bounds.right << ", \"bottom\": " << m.bounds.bottom << "}"
            << ", \"mode\": {\"width\": " << m.width << ", \"height\": " << m.height
            << ", \"refresh_hz\": " << m.refreshHz << "}"
            << ", \"dpi\": {\"x\": " << m.dpiX << ", \"y\": " << m.dpiY
            << ", \"scale_percent\": " << static_cast<unsigned>((m.dpiX * 100U + 48U) / 96U) << "}}";
        if (i + 1 != displayRecords.size()) out << ',';
        out << '\n';
    }
    out << "  ],\n";
    out << "  \"active_monitor_count\": " << displayRecords.size() << ",\n";

    out << "  \"gpu_adapters\": [";
    for (std::size_t i = 0; i < adapters.size(); ++i) {
        if (i != 0) out << ", ";
        out << quoted(utf8(adapters[i]));
    }
    out << "]\n";
    out << "}\n";
    return out.str();
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    std::filesystem::path outputPath;
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (arg == L"--out") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value after --out\n";
                return 2;
            }
            outputPath = argv[++i];
        } else if (arg == L"--version") {
            std::cout << visual::version::kSemanticVersion << '\n';
            return 0;
        } else {
            std::cerr << "Unknown argument\n";
            return 2;
        }
    }

    const std::string json = build_json();
    if (outputPath.empty()) {
        std::cout << json;
        return 0;
    }

    std::ofstream stream(outputPath, std::ios::binary | std::ios::trunc);
    if (!stream) {
        std::cerr << "Unable to create diagnostics output\n";
        return 3;
    }
    stream.write(json.data(), static_cast<std::streamsize>(json.size()));
    return stream ? 0 : 4;
}
