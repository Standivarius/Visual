# Visual alpha installation and update support

## Distribution model

The Visual alpha is packaged with Velopack under package id `Standivarius.Visual` on the `alpha` channel.

The intended tester flow is:

1. open the public Visual GitHub release page;
2. download the current `Visual-Setup.exe` asset;
3. run the installer;
4. launch Visual from its installed shortcut;
5. if Windows displays an unknown-publisher or SmartScreen warning, understand that early alpha builds are unsigned until production code signing is added.

## Requirements

Current alpha assumptions:

- Windows 11 development/test target;
- x64 Visual build;
- two active physical displays for the normal two-screen mode;
- Visual C++ runtime can be bootstrapped by the Velopack package.

## Package/version identity

- app id: `Standivarius.Visual`
- release channel: `alpha`
- semantic versions use the form `MAJOR.MINOR.PATCH-alpha.N`

## Update status

The release pipeline can create and publish Velopack update assets. In-app automatic check/download/apply is a separate integration step and must not be described as proven until an installed alpha has successfully updated to a later alpha in a controlled test.

Until then, testers should install a newer published alpha from its installer when specifically instructed.

## Installation troubleshooting

If Setup does not start:

1. confirm the downloaded file is the Visual release installer, not a source archive;
2. record the exact Windows warning/error text;
3. do not disable Windows security globally;
4. if the warning concerns an unknown publisher, confirm whether the build is one of the documented unsigned alpha releases;
5. if installation still fails, create a support report if `visual_diagnostics.exe` is available, otherwise escalate with the release version and exact installer error.

Do not recommend registry edits, policy bypasses, certificate installation or antivirus exclusions unless a reviewed Visual support procedure explicitly adds such a step later.
