#pragma once

namespace visual::setup {
// Runs only for an installed Velopack copy unless force is true. Returns 0 on
// success/not-needed and a non-zero setup-specific code when startup should stop.
int run_first_setup_if_needed(bool force);

// Velopack lifecycle helpers. The update hook migrates legacy setup evidence
// out of the replaceable install root when a verified legacy completion marker
// is still available. The uninstall hook removes only Visual-owned persistent data.
void migrate_legacy_state_after_update(const char* appVersion) noexcept;
void remove_persistent_state_before_uninstall(const char* appVersion) noexcept;
}
