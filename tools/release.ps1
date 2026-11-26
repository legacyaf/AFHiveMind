# Gets a Workshop release ready, and stops before anything untested can be published.
# Run it before every upload to the Workshop:
#   powershell -ExecutionPolicy Bypass -File tools\release.ps1
#
# It checks git (on main, nothing uncommitted, nothing new on GitHub), the version and its change
# notes, builds the mod, and runs the offline test, which is what compiles the scripts:
# tools\build.ps1 only packs files, so a script error would otherwise first show on a server.
# When everything passes it prints what to give the DayZ Tools Publisher and copies the change
# notes to the clipboard. It never publishes by itself.
#
#   -SkipGitCheck   build from another branch or uncommitted changes, to try something out.
#                   Not for a real release: what you publish would not match GitHub.
param([switch]$SkipGitCheck)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Set-Location $repo

function Stop-Release($why) {
    Write-Host ''
    Write-Host "RELEASE STOPPED: $why" -ForegroundColor Red
    Write-Host 'Nothing is ready to publish. Fix the above and run tools\release.ps1 again.' -ForegroundColor Red
    exit 1
}
function Step($text) { Write-Host ''; Write-Host "== $text" -ForegroundColor Cyan }

# 1. Git: publish only what is on GitHub's main.
Step 'Checking git'
if ($SkipGitCheck) {
    Write-Host 'Skipped (-SkipGitCheck): do not publish this build as a release.' -ForegroundColor Yellow
} else {
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Stop-Release 'git is not installed (winget install --id Git.Git -e).' }
    $branch = (git rev-parse --abbrev-ref HEAD).Trim()
    if ($branch -ne 'main') { Stop-Release "you are on branch '$branch', not main. Run: git checkout main; git pull" }
    $dirty = @(git status --porcelain -- source tools testmission workshop docs README.md '@AFHiveMind')
    if ($dirty.Count -gt 0) { Stop-Release ("there are uncommitted changes:`n" + ($dirty -join "`n")) }
    git fetch -q origin main
    if ($LASTEXITCODE -ne 0) { Stop-Release 'could not reach GitHub to check for new commits.' }
    $behind = [int](git rev-list --count HEAD..origin/main).Trim()
    if ($behind -gt 0) { Stop-Release "GitHub has $behind newer commit(s) on main. Run: git pull" }
    $ahead = [int](git rev-list --count origin/main..HEAD).Trim()
    if ($ahead -gt 0) { Stop-Release "$ahead local commit(s) are not on GitHub yet. Push them (git push) so the release matches GitHub." }
    Write-Host "On main, clean, up to date ($((git rev-parse --short HEAD).Trim()))."
}

# 2. Version and change notes.
Step 'Checking the version'
$cfg = Get-Content (Join-Path $repo 'source\AFHiveMind\config.cpp') -Raw
if ($cfg -notmatch 'version\s*=\s*"([^"]+)"') { Stop-Release 'no version found in source\AFHiveMind\config.cpp.' }
$version = $Matches[1]
$notes = Join-Path $repo "workshop\changenotes_${version}.txt"
if (-not (Test-Path $notes)) { Stop-Release "there are no change notes for ${version}: create workshop\changenotes_${version}.txt" }
$data = Get-Content (Join-Path $repo 'source\AFHiveMind\Scripts\3_Game\AFHiveMind\AFHM_Data.c') -Raw
if ($data -notmatch 'VERSION\s*=\s*"([^"]+)"' -or $Matches[1] -ne $version) {
    Stop-Release "AFHM_Const.VERSION in AFHM_Data.c is not ${version}. Set both to the same version."
}
Write-Host "Version ${version}, change notes found."

# 3. Build.
Step 'Building'
$started = Get-Date
try { & (Join-Path $PSScriptRoot 'build.ps1') } catch { Stop-Release "the build failed: $($_.Exception.Message)" }
$pbo = Join-Path $repo '@AFHiveMind\Addons\AFHiveMind.pbo'
if (-not (Test-Path $pbo) -or (Get-Item $pbo).LastWriteTime -lt $started) { Stop-Release 'the build did not write a new AFHiveMind.pbo.' }
if (-not (Get-ChildItem (Split-Path $pbo) -Filter '*.bisign')) { Stop-Release 'the PBO is not signed (keys\AFHiveMind.biprivatekey missing?). Servers would reject it.' }

# 4. Offline test: compiles every script and plays through the mod's features.
Step 'Running the offline test (about 2 minutes; leave the PC alone)'
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repo 'testmission\run_test.ps1') -Fresh
if ($LASTEXITCODE -ne 0) { Stop-Release 'the offline test failed. Do not publish; send the lines above to whoever is fixing it.' }

# 5. Ready.
$content = Join-Path $repo '@AFHiveMind'
try { Get-Content $notes -Raw | Set-Clipboard; $copied = ' (copied to the clipboard)' } catch { $copied = '' }
Write-Host ''
Write-Host "READY TO PUBLISH AF HiveMind ${version}" -ForegroundColor Green
Write-Host "  DayZ Tools Publisher > update the existing AF HiveMind item"
Write-Host "  Mod content address:  $content"
Write-Host "  Change notes:         $notes$copied"
Write-Host "  Afterwards: restart the servers that run the mod; the F9 report's first line should say ${version}."
exit 0
