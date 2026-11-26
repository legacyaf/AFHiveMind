# Starts the offline HiveMind test (mission AFHive, see its init.c), follows the script log
# until the mission prints TOUR DONE, and saves a picture of the DayZ window at each SHOT.
# Pictures go to testmission\captures. Needs Steam running and these Workshop mods
# installed: CF, Dabs Framework, DayZ-Expansion-Core, DayZ-Expansion-AI.
#   run_test.ps1          start with whatever brain.json the test profile already holds
#   run_test.ps1 -Fresh   delete the test profile's AFHiveMind folder first (empty brain)
# Ends with TEST PASSED (exit code 0) or TEST FAILED (exit code 1).
param([switch]$Fresh)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class AFWin {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
}
'@
[AFWin]::SetProcessDPIAware() | Out-Null

$here     = $PSScriptRoot
$repo     = Split-Path $here -Parent
$profiles = Join-Path $here 'profiles'
$out      = Join-Path $here 'captures'
$dayz     = 'C:\Program Files (x86)\Steam\steamapps\common\DayZ'
$workshop = Join-Path $dayz '!Workshop'
$mods     = @('@CF', '@Dabs Framework', '@DayZ-Expansion-Core', '@DayZ-Expansion-AI' | ForEach-Object { Join-Path $workshop $_ }) + (Join-Path $repo '@AFHiveMind')
foreach ($m in $mods) { if (-not (Test-Path -LiteralPath $m)) { throw "Mod folder missing: $m" } }

New-Item -ItemType Directory -Force $out, $profiles | Out-Null
Remove-Item (Join-Path $out '*.jpg') -ErrorAction SilentlyContinue
if ($Fresh) { Remove-Item (Join-Path $profiles 'AFHiveMind') -Recurse -Force -Confirm:$false -ErrorAction SilentlyContinue }

# DayZ on this PC is shared with other work: never close a game this script did not start.
if (Get-Process DayZDiag_x64 -ErrorAction SilentlyContinue) { throw 'DayZDiag is already running (someone else may be using it). Not starting the test.' }
$start = Get-Date
$game = Start-Process -PassThru (Join-Path $dayz 'DayZDiag_x64.exe') -WorkingDirectory $dayz -ArgumentList @(
    """-mission=$here\AFHive.ChernarusPlus""", """-mod=$($mods -join ';')""", """-profiles=$profiles""",
    '-window', '-nosplash', '-noPause', '-scriptDebug=true')

function Save-Window($name) {
    $game.Refresh()
    $h = $game.MainWindowHandle
    if ($h -eq 0) { return $false }
    $r = New-Object AFWin+RECT; [AFWin]::GetClientRect($h, [ref]$r) | Out-Null
    $w = $r.R - $r.L; $hgt = $r.B - $r.T
    if ($w -lt 100 -or $hgt -lt 100) { return $false }
    $bmp = New-Object System.Drawing.Bitmap $w, $hgt
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [AFWin]::PrintWindow($h, $hdc, 3) | Out-Null   # client area, full rendered content
    $g.ReleaseHdc($hdc)
    $g.Dispose()
    $bmp.Save((Join-Path $out "$name.jpg"), [System.Drawing.Imaging.ImageFormat]::Jpeg)
    $bmp.Dispose()
    return $true
}

$seen = @{}; $done = $false; $text = ''
$deadline = (Get-Date).AddSeconds(420)
while ((Get-Date) -lt $deadline -and -not $done) {
    $log = Get-ChildItem $profiles -Filter 'script_*.log' -ErrorAction SilentlyContinue |
        Where-Object LastWriteTime -gt $start | Sort-Object LastWriteTime | Select-Object -Last 1
    if ($log) {
        $fs = [IO.File]::Open($log.FullName, 'Open', 'Read', 'ReadWrite'); $sr = New-Object IO.StreamReader $fs
        $text = $sr.ReadToEnd(); $sr.Close(); $fs.Close()
        foreach ($m in [regex]::Matches($text, '\[AFHive\] SHOT (\S+)')) {
            $name = $m.Groups[1].Value
            if (-not $seen.ContainsKey($name)) {
                $seen[$name] = $true
                Start-Sleep -Milliseconds 800
                if (Save-Window $name) { "captured $name" } else { "could not capture $name" }
            }
        }
        if ($text -match 'TOUR DONE') { $done = $true }
        if ($text -match 'ompile error|Can.t compile') { break }
    }
    if ($game.HasExited) { 'DayZ exited'; break }
    Start-Sleep -Milliseconds 300
}
$lines = @(($text -split "`r?`n" | Select-String '\[AFHive\]|\[AFHiveMind\]|ompile|SCRIPT\s+\(E\)|SCRIPT ERROR').Line)
$lines
"test finished: $done; pictures: $($seen.Count)"
if (-not $game.HasExited) { Stop-Process -Id $game.Id -Force -Confirm:$false }

# A clear verdict and exit code, so tools\release.ps1 can stop on a failure.
$failed = @($lines | Where-Object { $_ -cmatch 'FAILED' -or $_ -match 'ompile error|Can.t compile' })
# A script error during the run fails it when the error or its next few lines (the call stack)
# mention this mod. Other mods' errors are shown as warnings only.
$scriptErrors = [regex]::Matches($text, '(?m)^.*SCRIPT\s*(\(E\)|ERROR).*(\r?\n.*){0,8}')
foreach ($e in $scriptErrors) {
    if ($e.Value -match 'AFHiveMind|AFHM_|AFHive') { $failed += $e.Value.Trim() }
    else { 'warning, script error from another mod: ' + ($e.Value -split "`r?`n")[0] }
}
if ($failed.Count -gt 0) { 'Failures:'; $failed }
if (-not $done -or $failed.Count -gt 0) {
    if (-not $done) { 'the mission did not reach TOUR DONE (script error, crash or timeout)' }
    'TEST FAILED'
    exit 1
}
'TEST PASSED'
exit 0
