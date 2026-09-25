#include <windows.h>
#include <shellapi.h>
#include "Velopack.hpp"
#include "setup_orchestrator.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

// main.cpp is compiled with wWinMain renamed to VisualProductMain. The compiler
// definition also renames the CRT/header wWinMain declaration, so the resulting
// implementation has C linkage; match that linkage explicitly here.
extern "C" int WINAPI VisualProductMain(HINSTANCE instance, HINSTANCE previousInstance, PWSTR commandLine, int showCommand);

namespace {

constexpr char kUpdateRepository[] = "https://github.com/Standivarius/Visual";

struct MaintenanceOptions {
    bool checkUpdate{};
    bool applyUpdate{};
};

bool has_command_line_argument(const wchar_t* expected) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return false;
    bool found = false;
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], expected) == 0) { found = true; break; }
    }
    LocalFree(argv);
    return found;
}
MaintenanceOptions parse_maintenance_options() {
    MaintenanceOptions result{};
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return result;
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (arg == L"--update-check") result.checkUpdate = true;
        else if (arg == L"--update-now") result.applyUpdate = true;
    }
    LocalFree(argv);
    return result;
}

std::filesystem::path update_log_path() {
    wchar_t localAppData[32768]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, static_cast<DWORD>(std::size(localAppData)));
    std::filesystem::path root;
    if (length > 0 && length < std::size(localAppData)) {
        root = std::filesystem::path(localAppData) / L"Standivarius.Visual";
    } else {
        wchar_t tempPath[MAX_PATH]{};
        const DWORD tempLength = GetTempPathW(ARRAYSIZE(tempPath), tempPath);
        if (tempLength == 0 || tempLength >= ARRAYSIZE(tempPath)) {
            throw std::runtime_error("Unable to resolve a directory for the Visual update log");
        }
        root = std::filesystem::path(tempPath) / L"Standivarius.Visual";
    }
    std::filesystem::create_directories(root);
    return root / L"visual_update.log";
}

void append_update_log(const std::filesystem::path& path, const std::string& message) {
    SYSTEMTIME now{};
    GetSystemTime(&now);
    std::ofstream stream(path, std::ios::out | std::ios::app);
    if (!stream) return;
    char timestamp[64]{};
    sprintf_s(timestamp, "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
              now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
    stream << timestamp << ' ' << message << '\n';
    stream.flush();
}
bool equals_ci(const std::wstring& left, const wchar_t* right) noexcept {
    return _wcsicmp(left.c_str(), right) == 0;
}

bool updates_managed_by_it() noexcept {
    wchar_t envValue[64]{};
    const DWORD envLength = GetEnvironmentVariableW(L"VISUAL_UPDATE_MODE", envValue, ARRAYSIZE(envValue));
    if (envLength > 0 && envLength < ARRAYSIZE(envValue) && equals_ci(envValue, L"it-managed")) return true;

    wchar_t registryValue[64]{};
    DWORD bytes = sizeof(registryValue);
    const LSTATUS status = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Standivarius\\Visual",
        L"UpdateMode",
        RRF_RT_REG_SZ,
        nullptr,
        registryValue,
        &bytes);
    return status == ERROR_SUCCESS && equals_ci(registryValue, L"ITManaged");
}

int run_update_maintenance(const MaintenanceOptions& options) noexcept {
    std::filesystem::path logPath;
    try {
        logPath = update_log_path();
        append_update_log(logPath, options.applyUpdate ? "update_now_start" : "update_check_start");
        if (updates_managed_by_it()) {
            append_update_log(logPath, "result=disabled_by_policy update_mode=it_managed");
            return 43;
        }

        auto source = std::make_unique<Velopack::GithubSource>(kUpdateRepository, "", true);
        Velopack::UpdateManager manager(std::move(source));
        append_update_log(logPath, "installed_version=" + manager.GetCurrentVersion());

        if (manager.IsPortable()) {
            append_update_log(logPath, "result=not_installed_or_portable");
            return 40;
        }

        const auto update = manager.CheckForUpdates();
        if (!update.has_value()) {
            append_update_log(logPath, "result=no_update");
            return 0;
        }

        append_update_log(logPath, "available_version=" + update->TargetFullRelease.Version);
        if (!options.applyUpdate) {
            append_update_log(logPath, "result=update_available");
            return 10;
        }

        append_update_log(logPath, "download_start");
        manager.DownloadUpdates(update.value());
        append_update_log(logPath, "download_complete");

        // Prepare Update.exe to apply the downloaded package after this process exits.
        // No automatic restart is requested here; a later user-facing update workflow
        // can decide when/how to restart Visual without changing the update transport.
        manager.WaitExitThenApplyUpdates(update.value(), false, false);
        append_update_log(logPath, "result=apply_scheduled");
        return 0;
    } catch (const std::exception& e) {
        if (!logPath.empty()) append_update_log(logPath, std::string("result=error message=") + e.what());
        return 41;
    } catch (...) {
        if (!logPath.empty()) append_update_log(logPath, "result=unknown_error");
        return 42;
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previousInstance, PWSTR commandLine, int showCommand) {
    // This must remain the first meaningful Visual statement. During Velopack's
    // install/update/uninstall fast hooks Run() may terminate the process here.
    Velopack::VelopackApp::Build().Run();

    const auto maintenance = parse_maintenance_options();
    if (maintenance.applyUpdate || maintenance.checkUpdate) {
        return run_update_maintenance(maintenance);
    }

    // The bounded graphics probe is launched by the setup orchestrator itself;
    // never recurse into setup while that child process is running.
    const bool healthCheck = has_command_line_argument(L"--health-check");
    const bool skipSetup = has_command_line_argument(L"--skip-setup");
    const bool forceSetup = has_command_line_argument(L"--setup-check");
    if (!healthCheck && !skipSetup) {
        const int setupResult = visual::setup::run_first_setup_if_needed(forceSetup);
        if (setupResult != 0 || forceSetup) return setupResult;
    }

    return VisualProductMain(instance, previousInstance, commandLine, showCommand);
}
