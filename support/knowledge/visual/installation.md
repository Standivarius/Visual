# Visual alpha installation and update support

## Distribution

Visual alpha uses package id `Standivarius.Visual` on the Velopack `alpha` channel.

Tester flow:

1. download the current Setup executable from the public Visual GitHub prerelease;
2. run Setup;
3. launch Visual from the installed shortcut;
4. expect possible SmartScreen/unknown-publisher warnings while alpha packages remain unsigned.

Current target assumptions are Windows 11, x64, and two active physical displays for the normal two-screen mode.

## Lifecycle status

Published `0.1.0-alpha.1` bypassed Velopack application validation and did not initialize the native lifecycle handler. A physical alpha.1 install showed `Install Partially Succeeded` even though the application could subsequently run.

`0.1.0-alpha.2` integrates Velopack 1.2.0 at the real process entry point and packages the required native runtime as `velopack_libc.dll`.

Local alpha.2 verification on 2026-09-24 proved:

- normal packaging with no `--skipVeloAppCheck`;
- clean Setup exit `0`;
- Setup log reports `Hook executed successfully`;
- installed ProductVersion `0.1.0-alpha.2`;
- installed `Update.exe` and `velopack_libc.dll` are present;
- installed `visual_app.exe --update-check` exits `0` and reaches `result=no_update` against the current public feed;
- consolidated lifecycle verification ends in `LIFECYCLE_RESULT=PASS`.

The exact historical alpha.1 root cause remains formally unproven because its Setup log was not recovered. Missing lifecycle integration is the leading explanation.

## Update maintenance

Engineering commands in installed alpha.2:

- `visual_app.exe --update-check` - check public GitHub prereleases without starting the magnifier UI;
- `visual_app.exe --update-now` - check, download and schedule an available update to apply after Visual exits; no automatic restart is requested yet.

Maintenance diagnostics are appended to `%LOCALAPPDATA%\Standivarius.Visual\visual_update.log`.

Current proof boundary:

- packaging/feed production: proven;
- lifecycle startup: proven locally for alpha.2;
- installed update check: proven locally for alpha.2;
- download/apply: implemented, awaiting a later public alpha for end-to-end proof;
- polished automatic-update UX/policy: not implemented.

Alpha.1 cannot initiate its own update to alpha.2 because it has no update client. Once alpha.2 is public, a later alpha can exercise the installed `--update-now` path.

## Troubleshooting installation

If Setup fails or reports partial success:

1. record the exact status/error text;
2. preserve the Setup/Velopack log before retrying;
3. confirm the downloaded file is the Visual Setup executable rather than a source archive;
4. do not disable Windows security globally;
5. treat documented unsigned-alpha publisher warnings separately from installer failure;
6. record the installed ProductVersion if files were installed despite the warning;
7. use `visual_diagnostics.exe` or the repository support-bundle procedure when available.

Do not recommend registry edits, policy bypasses, certificate installation or antivirus exclusions unless a reviewed Visual support procedure explicitly requires them.
