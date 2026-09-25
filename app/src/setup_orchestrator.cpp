#include "setup_orchestrator.h"

#include "visual_cloud_config.h"
#include "visual_version.h"

#include <windows.h>
#include <winhttp.h>
#include <winternl.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace visual::setup {
namespace {

constexpr int kSetupHealthTimeoutMs = 7000;
constexpr DWORD kChildWaitMs = 20000;
constexpr DWORD kSetupMutexWaitMs = 180000;
constexpr wchar_t kSetupMutexName[] = L"Local\\Standivarius.Visual.Setup.v1";

struct SetupMutexGuard {
    HANDLE handle{};
    bool owned{};

    ~SetupMutexGuard() {
        if (owned && handle) ReleaseMutex(handle);
        if (handle) CloseHandle(handle);
    }

    SetupMutexGuard(const SetupMutexGuard&) = delete;
    SetupMutexGuard& operator=(const SetupMutexGuard&) = delete;
    SetupMutexGuard() = default;
};

std::filesystem::path module_path() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) return {};
    buffer.resize(length);
    return std::filesystem::path(buffer);
}

std::filesystem::path setup_root() {
    wchar_t localAppData[32768]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, static_cast<DWORD>(std::size(localAppData)));
    if (length == 0 || length >= std::size(localAppData)) return {};
    auto root = std::filesystem::path(localAppData) / L"Standivarius.Visual" / L"doxa-setup";
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    return ec ? std::filesystem::path{} : root;
}

bool installed_copy() {
    const auto self = module_path();
    if (self.empty()) return false;
    const auto current = self.parent_path();
    if (_wcsicmp(current.filename().c_str(), L"current") != 0) return false;
    return std::filesystem::exists(current.parent_path() / L"Update.exe");
}

void append_log(const std::string& message) {
    const auto root = setup_root();
    if (root.empty()) return;
    SYSTEMTIME now{};
    GetSystemTime(&now);
    char timestamp[64]{};
    sprintf_s(timestamp, "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
              now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
    std::ofstream stream(root / L"setup.log", std::ios::out | std::ios::app);
    if (stream) stream << timestamp << ' ' << message << '\n';
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

BOOL CALLBACK count_monitor(HMONITOR, HDC, LPRECT, LPARAM data) {
    auto* count = reinterpret_cast<unsigned*>(data);
    ++(*count);
    return TRUE;
}

unsigned active_monitor_count() {
    unsigned count = 0;
    EnumDisplayMonitors(nullptr, nullptr, count_monitor, reinterpret_cast<LPARAM>(&count));
    return count;
}

bool user_is_admin() {
    BOOL isMember = FALSE;
    PSID administrators = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administrators)) {
        return false;
    }
    CheckTokenMembership(nullptr, administrators, &isMember);
    FreeSid(administrators);
    return isMember == TRUE;
}

bool registry_key_exists(HKEY root, const wchar_t* subkey) {
    HKEY key = nullptr;
    const LSTATUS status = RegOpenKeyExW(root, subkey, 0, KEY_READ, &key);
    if (key) RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

bool pending_servicing_reboot() {
    return registry_key_exists(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\RebootPending") ||
           registry_key_exists(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\RebootRequired");
}

std::wstring quote_arg(const std::wstring& value) {
    std::wstring result = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'\"') result += L'\\';
        result += ch;
    }
    result += L"\"";
    return result;
}

int run_health_probe(const std::filesystem::path& telemetryPath) {
    const auto self = module_path();
    if (self.empty()) return 32;
    std::wostringstream cmd;
    cmd << quote_arg(self.wstring())
        << L" --health-check --health-timeout-ms " << kSetupHealthTimeoutMs
        << L" --zoom 2 --log " << quote_arg(telemetryPath.wstring());
    std::wstring command = cmd.str();
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        self.parent_path().c_str(), &startup, &process)) {
        append_log("health_probe_create_failed win32=" + std::to_string(GetLastError()));
        return 33;
    }
    CloseHandle(process.hThread);
    const DWORD wait = WaitForSingleObject(process.hProcess, kChildWaitMs);
    if (wait == WAIT_TIMEOUT) {
        TerminateProcess(process.hProcess, 34);
        CloseHandle(process.hProcess);
        append_log("health_probe_timeout");
        return 34;
    }
    DWORD exitCode = 35;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    append_log("health_probe_exit=" + std::to_string(exitCode));
    return static_cast<int>(exitCode);
}

std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (unsigned char ch : value) {
        switch (ch) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << static_cast<char>(ch); break;
        }
    }
    return out.str();
}

std::string build_state_json(bool healthPassed) {
    const auto os = windows_version();
    const unsigned monitors = active_monitor_count();
    std::ostringstream out;
    out << "{\"schema_version\":\"1\",\"client_id\":\"visual-alpha-" << GetCurrentProcessId() << '-' << GetTickCount64() << "\",\"state\":{";
    out << "\"mode\":\"normal_first_install\",";
    out << "\"doxa_hardware\":\"expected\",";
    out << "\"hardware_profile\":\"surrogate-two-monitor\",";
    out << "\"expected_display_count\":2,";
    out << "\"windows_supported\":" << ((os.dwMajorVersion > 10 || (os.dwMajorVersion == 10 && os.dwBuildNumber >= 19041)) ? "true" : "false") << ',';
    out << "\"windows_build\":" << os.dwBuildNumber << ',';
    out << "\"enterprise_managed\":false,";
    out << "\"deployment_context\":\"user\",";
    out << "\"user_is_admin\":" << (user_is_admin() ? "true" : "false") << ',';
    out << "\"active_displays\":" << monitors << ',';
    out << "\"driver_state\":\"approved\",";
    out << "\"driver_version\":\"surrogate-host-stack\",";
    out << "\"firmware_version\":\"not-applicable-surrogate\",";
    out << "\"pending_reboot\":" << (pending_servicing_reboot() ? "true" : "false") << ',';
    out << "\"app_control\":\"allow\",";
    out << "\"network_to_doxa_cloud\":\"reachable\",";
    out << "\"wgc_health\":\"" << (healthPassed ? "pass" : "fail") << "\",";
    out << "\"visual_installed\":true,";
    out << "\"visual_version\":\"" << json_escape(visual::version::kSemanticVersion) << "\",";
    out << "\"post_install_health\":\"" << (healthPassed ? "pass" : "fail") << "\"}}";
    return out.str();
}

std::wstring widen_utf8(const std::string& value) {
    if (value.empty()) return {};
    const int needed = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring result(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), needed);
    return result;
}

std::string narrow_utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string result(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), needed, nullptr, nullptr);
    return result;
}

struct HttpResult {
    DWORD status{};
    std::string body;
    std::string error;
};

HttpResult post_json(const std::string& body) {
    HttpResult result{};
    const std::wstring baseUrl = cloud::kPlannerBaseUrl;
    if (baseUrl.empty()) {
        result.error = "planner_url_missing";
        return result;
    }

    URL_COMPONENTSW parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[512]{};
    wchar_t path[2048]{};
    parts.lpszHostName = host;
    parts.dwHostNameLength = ARRAYSIZE(host);
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = ARRAYSIZE(path);
    if (!WinHttpCrackUrl(baseUrl.c_str(), 0, 0, &parts)) {
        result.error = "WinHttpCrackUrl=" + std::to_string(GetLastError());
        return result;
    }
    std::wstring hostName(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring basePath(parts.lpszUrlPath, parts.dwUrlPathLength);
    while (!basePath.empty() && basePath.back() == L'/') basePath.pop_back();
    const std::wstring requestPath = basePath + L"/v1/plan";

    HINTERNET session = WinHttpOpen(L"Visual Doxa Setup/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        result.error = "WinHttpOpen=" + std::to_string(GetLastError());
        return result;
    }
    WinHttpSetTimeouts(session, 10000, 10000, 10000, 90000);
    HINTERNET connect = WinHttpConnect(session, hostName.c_str(), parts.nPort, 0);
    if (!connect) {
        result.error = "WinHttpConnect=" + std::to_string(GetLastError());
        WinHttpCloseHandle(session);
        return result;
    }
    const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"POST", requestPath.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        result.error = "WinHttpOpenRequest=" + std::to_string(GetLastError());
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return result;
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    if (cloud::kClientToken[0] != L'\0') {
        headers += L"Authorization: Bearer ";
        headers += cloud::kClientToken;
        headers += L"\r\n";
    }
    const BOOL sent = WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(-1L),
                                         const_cast<char*>(body.data()), static_cast<DWORD>(body.size()),
                                         static_cast<DWORD>(body.size()), 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        result.error = "WinHTTP_send_or_receive=" + std::to_string(GetLastError());
    } else {
        DWORD statusSize = sizeof(result.status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &result.status, &statusSize, WINHTTP_NO_HEADER_INDEX);
        for (;;) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available) || available == 0) break;
            std::string chunk(static_cast<std::size_t>(available), '\0');
            DWORD read = 0;
            if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0) break;
            chunk.resize(read);
            result.body += chunk;
            if (result.body.size() > 262144) break;
        }
    }
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return result;
}

std::string json_string_field(const std::string& json, const std::string& field) {
    const std::string needle = "\"" + field + "\"";
    const auto key = json.find(needle);
    if (key == std::string::npos) return {};
    const auto colon = json.find(':', key + needle.size());
    if (colon == std::string::npos) return {};
    auto start = json.find('"', colon + 1);
    if (start == std::string::npos) return {};
    ++start;
    std::string value;
    bool escaped = false;
    for (std::size_t i = start; i < json.size(); ++i) {
        const char ch = json[i];
        if (escaped) {
            switch (ch) {
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            default: value.push_back(ch); break;
            }
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == '"') {
            break;
        } else {
            value.push_back(ch);
        }
    }
    return value;
}

void write_evidence(const std::string& state, const HttpResult& cloudResult) {
    const auto root = setup_root();
    if (root.empty()) return;
    std::ofstream stateFile(root / L"state.json", std::ios::binary | std::ios::trunc);
    if (stateFile) stateFile << state << '\n';
    std::ofstream responseFile(root / L"cloud-response.json", std::ios::binary | std::ios::trunc);
    if (responseFile) responseFile << cloudResult.body << '\n';
}

bool write_marker() {
    const auto root = setup_root();
    if (root.empty()) return false;
    std::ofstream marker(root / L"complete-v1.txt", std::ios::out | std::ios::trunc);
    marker << "visual_version=" << visual::version::kSemanticVersion << '\n';
    marker << "status=ok\n";
    return static_cast<bool>(marker);
}

bool marker_exists() {
    const auto root = setup_root();
    return !root.empty() && std::filesystem::exists(root / L"complete-v1.txt");
}

void show_message(const std::wstring& text, UINT flags = MB_OK | MB_ICONINFORMATION) {
    MessageBoxW(nullptr, text.c_str(), L"Visual Setup", flags | MB_SETFOREGROUND);
}

int complete_setup(const std::string& detail) {
    append_log("setup_complete " + detail);
    if (!write_marker()) append_log("marker_write_failed");
    show_message(L"Visual setup is complete.\n\nTwo-monitor graphics checks passed and Doxa cloud support is connected.\n\nVisual will now start.");
    return 0;
}

} // namespace

int run_first_setup_if_needed(bool force) {
    if (!force && !installed_copy()) return 0;
    if (!force && marker_exists()) return 0;

    SetupMutexGuard setupMutex;
    setupMutex.handle = CreateMutexW(nullptr, FALSE, kSetupMutexName);
    if (!setupMutex.handle) {
        append_log("setup_mutex_create_failed win32=" + std::to_string(GetLastError()));
        show_message(L"Visual could not coordinate first-run setup with another Visual process.", MB_OK | MB_ICONERROR);
        return 56;
    }

    DWORD wait = WaitForSingleObject(setupMutex.handle, 0);
    if (wait == WAIT_TIMEOUT) {
        append_log("setup_mutex_wait");
        wait = WaitForSingleObject(setupMutex.handle, kSetupMutexWaitMs);
    }
    if (wait == WAIT_OBJECT_0) {
        setupMutex.owned = true;
    } else if (wait == WAIT_ABANDONED) {
        setupMutex.owned = true;
        append_log("setup_mutex_abandoned");
    } else if (wait == WAIT_TIMEOUT) {
        append_log("setup_mutex_timeout");
        show_message(L"Another Visual setup process is still running. Please wait for it to finish, then start Visual again.", MB_OK | MB_ICONWARNING);
        return 57;
    } else {
        append_log("setup_mutex_wait_failed win32=" + std::to_string(GetLastError()));
        show_message(L"Visual could not coordinate first-run setup with another Visual process.", MB_OK | MB_ICONERROR);
        return 58;
    }

    if (!force && marker_exists()) {
        append_log("setup_peer_completed");
        return 0;
    }

    append_log(std::string("setup_start version=") + visual::version::kSemanticVersion);
    const auto root = setup_root();
    if (root.empty()) {
        show_message(L"Visual could not create its setup state directory.", MB_OK | MB_ICONERROR);
        return 50;
    }

    const int initialHealth = run_health_probe(root / L"health.csv");
    const bool healthPassed = initialHealth == 0;
    wchar_t exerciseValue[8]{};
    const DWORD exerciseLength = GetEnvironmentVariableW(L"VISUAL_SETUP_EXERCISE_AI", exerciseValue, ARRAYSIZE(exerciseValue));
    const bool exerciseAi = exerciseLength > 0 && exerciseLength < ARRAYSIZE(exerciseValue) && _wcsicmp(exerciseValue, L"1") == 0;
    if (exerciseAi) append_log("engineering_exercise_ai=true");
    const std::string state = build_state_json(healthPassed && !exerciseAi);
    const HttpResult cloudResult = post_json(state);
    write_evidence(state, cloudResult);

    if (!cloudResult.error.empty() || cloudResult.status != 200) {
        append_log("cloud_unavailable status=" + std::to_string(cloudResult.status) + " error=" + cloudResult.error);
        if (healthPassed) {
            if (!write_marker()) append_log("marker_write_failed");
            show_message(L"Visual passed its local graphics checks, but Doxa cloud support could not be reached.\n\nVisual will start normally. Cloud diagnostics can be retried later.", MB_OK | MB_ICONWARNING);
            return 0;
        }
        show_message(L"Visual could not complete the graphics check and Doxa cloud support is currently unavailable.\n\nPlease check the Internet connection and start Visual again.", MB_OK | MB_ICONERROR);
        return 51;
    }

    const std::string outcome = json_string_field(cloudResult.body, "outcome");
    const std::string action = json_string_field(cloudResult.body, "action");
    const std::string provider = json_string_field(cloudResult.body, "provider");
    const std::string reason = json_string_field(cloudResult.body, "reason");
    const std::string configuredModelId = json_string_field(cloudResult.body, "configured_model_id");
    const std::string difyMessageId = json_string_field(cloudResult.body, "dify_message_id");
    const std::string difyConversationId = json_string_field(cloudResult.body, "dify_conversation_id");
    const std::string difyTaskId = json_string_field(cloudResult.body, "dify_task_id");
    append_log("cloud_decision outcome=" + outcome + " action=" + action + " provider=" + provider);
    if (outcome == "ai_plan") {
        std::string trace = "cloud_ai_trace";
        if (!configuredModelId.empty()) trace += " configured_model_id=" + configuredModelId;
        if (!difyMessageId.empty()) trace += " dify_message_id=" + difyMessageId;
        if (!difyConversationId.empty()) trace += " dify_conversation_id=" + difyConversationId;
        if (!difyTaskId.empty()) trace += " dify_task_id=" + difyTaskId;
        append_log(trace);
    }

    if (outcome == "installed_ok") return complete_setup("provider=" + provider);

    if (outcome == "ai_plan") {
        if (action == "run_minimal_capture_probe" || action == "retry_post_install_health" || action == "recheck_doxa_devices") {
            const int followUp = run_health_probe(root / L"health-followup.csv");
            if (followUp == 0) return complete_setup("cloud_recovery action=" + action + " provider=" + provider);
            append_log("cloud_followup_failed action=" + action + " exit=" + std::to_string(followUp));
            const std::wstring why = widen_utf8(reason);
            show_message(L"Doxa cloud diagnostics ran an approved follow-up check, but the graphics path is still not healthy.\n\n" + why + L"\n\nStart Visual again after checking the display connection.", MB_OK | MB_ICONERROR);
            return 52;
        }
        if (action == "collect_support_bundle") {
            show_message(L"Doxa cloud diagnostics could not resolve the installation automatically.\n\nDiagnostic evidence has been saved under Local AppData\\Standivarius.Visual\\doxa-setup.", MB_OK | MB_ICONWARNING);
            return 53;
        }
        if (action == "escalate_it") {
            show_message(L"Doxa cloud diagnostics determined that this installation needs IT assistance.\n\n" + widen_utf8(reason), MB_OK | MB_ICONWARNING);
            return 54;
        }
        append_log("cloud_action_rejected action=" + action);
        show_message(L"Doxa cloud returned an unsupported setup action. Visual stopped safely.", MB_OK | MB_ICONERROR);
        return 55;
    }

    if (outcome == "waiting_for_display") {
        show_message(L"Visual needs two active displays in Windows Extended mode.\n\nConnect or enable the second display, then start Visual again.", MB_OK | MB_ICONWARNING);
        return 56;
    }
    if (outcome == "reboot_required") {
        show_message(L"Windows reports that a servicing reboot is required.\n\nRestart Windows, then start Visual again.", MB_OK | MB_ICONWARNING);
        return 57;
    }
    if (outcome == "needs_it") {
        show_message(L"This installation is blocked by an enterprise policy and needs IT assistance.", MB_OK | MB_ICONWARNING);
        return 58;
    }
    if (outcome == "unsupported_os") {
        show_message(L"This Windows version is outside the current Visual test baseline.", MB_OK | MB_ICONERROR);
        return 59;
    }

    append_log("unhandled_cloud_outcome=" + outcome + " action=" + action);
    show_message(L"Visual setup could not complete. Please start Visual again after checking the display connection.", MB_OK | MB_ICONERROR);
    return 60;
}

} // namespace visual::setup
