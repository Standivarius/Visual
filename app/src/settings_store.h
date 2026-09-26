#pragma once

#include "core/settings_model.h"

#include <filesystem>

namespace visual::settings {

[[nodiscard]] std::filesystem::path default_settings_path();
[[nodiscard]] visual::core::VisualSettings load_settings(const std::filesystem::path& path);
[[nodiscard]] bool save_settings(const std::filesystem::path& path, const visual::core::VisualSettings& settings) noexcept;

} // namespace visual::settings
