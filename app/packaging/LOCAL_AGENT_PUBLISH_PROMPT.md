# Local-agent prompt: publish the first Visual alpha

Use this only from the canonical repository:

`C:\dev\Visual`

The user has explicitly authorized commits/pushes only to `Standivarius/Visual` for the Visual alpha distribution work.

## Prompt for Codex / Antigravity

> Work only in `C:\dev\Visual`. Do not modify other repositories or GitHub organization settings. The public GitHub repository is `https://github.com/Standivarius/Visual` and is currently empty.
>
> First inspect `app/packaging/bootstrap-public-alpha.ps1`, `.github/workflows/release-alpha.yml`, `app/packaging/package.ps1`, and current `git status`. Do not sweep unrelated bridge/docs/lab/artifact work into the public commit.
>
> Verify local GitHub authentication is available. If authentication requires interactive user consent, stop and ask the user to complete that login; do not request or print passwords/tokens.
>
> Then execute:
>
> `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\app\packaging\bootstrap-public-alpha.ps1`
>
> The script must stage only `.gitignore`, `.config/dotnet-tools.json`, `.github/workflows/release-alpha.yml`, the explicit Visual application/build/source/test/packaging paths, and `support/`. It must leave unrelated `bridge/**`, `docs/**`, `lab/**`, `artifacts/**`, `STATUS.md`, and `scripts/**` changes unstaged.
>
> After push, verify that tag `v0.1.0-alpha.1` exists on `origin` and that GitHub Actions starts `Release Visual Alpha`.
>
> Wait for the workflow to finish. If it fails, inspect the workflow log and make the smallest correction only within `.github/workflows/release-alpha.yml`, `app/packaging/**`, `.config/dotnet-tools.json`, `app/CMakeLists.txt`, `app/build.ps1`, or the new diagnostics/version source files. Commit/push that correction and create a new alpha tag rather than rewriting the published tag if the original tag already triggered a public release.
>
> On success, report the GitHub release URL and the exact installer asset filename. Do not claim installation is proven until the installer has subsequently been installed on a Windows machine.

## Expected result

The first successful run should create a public GitHub prerelease for:

`v0.1.0-alpha.1`

The release should contain a Velopack Windows Setup executable plus the corresponding package/feed assets.
