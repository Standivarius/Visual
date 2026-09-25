#pragma once

namespace visual::setup {
// Runs only for an installed Velopack copy unless force is true. Returns 0 on
// success/not-needed and a non-zero setup-specific code when startup should stop.
int run_first_setup_if_needed(bool force);
}
