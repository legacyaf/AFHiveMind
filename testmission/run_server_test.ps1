# Real-bullet test: starts DayZDiag as a local dedicated server (mission AFHiveMP, see its
# init.c) and a DayZDiag client that connects to it, follows the server's script log until
# TOUR DONE, and saves a picture of the client window at each SHOT (testmission\captures).
# Needs Steam running and the Workshop mods CF, Dabs Framework, DayZ-Expansion-Core and
# DayZ-Expansion-AI. Builds the test-only helper mod @AFHiveTest first.
# It brings the client window to the front and clicks the left mouse button in it for the
# shooting step: leave the mouse and keyboard alone while it runs (about five minutes).
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
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
}
'@
[AFWin]::SetProcessDPIAware() | Out-Null

$here     = $PSScriptRoot
$repo     = Split-Path $here -Parent
$tools    = 'C:\Program Files (x86)\Steam\steamapps\common\DayZ Tools'
$dayz     = 'C:\Program Files (x86)\Steam\steamapps\common\DayZ'
$exe      = Join-Path $dayz 'DayZDiag_x64.exe'
$workshop = Join-Path $dayz '!Workshop'
$srvProf  = Join-Path $here 'profiles_server'
$cliProf  = Join-Path $here 'profiles_client'
$out      = Join-Path $here 'captures'

# DayZ on this PC is shared with other work: never close a game this script did not start.
if (Get-Process DayZDiag_x64 -ErrorAction SilentlyContinue) { throw 'DayZDiag is already running (someone else may be using it). Not starting the test.' }

# The helper mod: scripts only, packed as they are, never signed.
$helper = Join-Path $here '@AFHiveTest\Addons'
$temp   = Join-Path $env:TEMP 'AFHiveTest_build'
New-Item -ItemType Directory -Force $helper, $temp, $out, $srvProf, $cliProf | Out-Null
$p = Start-Process (Join-Path $tools 'Bin\AddonBuilder\AddonBuilder.exe') -ArgumentList @(
    "`"$(Join-Path $here 'testmod\AFHiveTest')`"", "`"$helper`"", '-clear', '-packonly', '-prefix=AFHiveTest',
    "-project=`"$(Join-Path $here 'testmod')`"", "-temp=`"$temp`"", "-toolsDirectory=`"$tools`""
) -RedirectStandardOutput (Join-Path $temp 'addonbuilder.log') -NoNewWindow -PassThru -Wait
if ($p.ExitCode -ne 0 -or -not (Test-Path (Join-Path $helper 'AFHiveTest.pbo'))) { throw 'Could not build the helper mod.' }

$mods = @('@CF', '@Dabs Framework', '@DayZ-Expansion-Core', '@DayZ-Expansion-AI' | ForEach-Object { Join-Path $workshop $_ }) +
    (Join-Path $repo '@AFHiveMind') + (Join-Path $here '@AFHiveTest')
foreach ($m in $mods) { if (-not (Test-Path -LiteralPath $m)) { throw "Mod folder missing: $m" } }
$modArg = """-mod=$($mods -join ';')"""

# The mission the server runs is put together in run_mp (not in git): Bohemia's stock
# Chernarus mission files (https://github.com/BohemiaInteractive/DayZ-Central-Economy, cloned
# to $stock) with this test's init.c, and with infected and animals switched off.
$stock   = 'C:\DayZMods\_external\DayZ-Central-Economy\dayzOffline.chernarusplus'
$mission = Join-Path $here 'run_mp\AFHiveMP.ChernarusPlus'
if (-not (Test-Path $stock)) { throw "Stock mission files missing: clone DayZ-Central-Economy so that $stock exists." }
if (-not (Test-Path (Join-Path $mission 'db\globals.xml'))) {
    New-Item -ItemType Directory -Force $mission | Out-Null
    Copy-Item (Join-Path $stock '*') $mission -Recurse -Force
    $globals = Join-Path $mission 'db\globals.xml'
    $g = [IO.File]::ReadAllText($globals) -replace '(name="(Zombie|Animal)MaxCount" type="0" value=")\d+', '${1}0'
    [IO.File]::WriteAllText($globals, $g, (New-Object Text.UTF8Encoding $false))
}
Copy-Item (Join-Path $here 'AFHiveMP.ChernarusPlus\init.c') (Join-Path $mission 'init.c') -Force
# Every run starts with an empty brain and no stored character.
Remove-Item (Join-Path $srvProf 'AFHiveMind') -Recurse -Force -Confirm:$false -ErrorAction SilentlyContinue
Get-ChildItem $mission -Directory -Filter 'storage_*' | Remove-Item -Recurse -Force -Confirm:$false
Remove-Item (Join-Path $out 'mp_*.jpg') -ErrorAction SilentlyContinue

$start = Get-Date
$server = Start-Process $exe -WorkingDirectory $dayz -PassThru -ArgumentList @(
    '-server', """-config=$here\serverDZ.cfg""", '-port=2302', """-mission=$mission""",
    $modArg, """-profiles=$srvProf""", '-doLogs', '-scriptDebug=true')

function Read-Log($dir) {
    $log = Get-ChildItem $dir -Filter 'script_*.log' -ErrorAction SilentlyContinue |
        Where-Object LastWriteTime -gt $start | Sort-Object LastWriteTime | Select-Object -Last 1
    if (-not $log) { return '' }
    $fs = [IO.File]::Open($log.FullName, 'Open', 'Read', 'ReadWrite'); $sr = New-Object IO.StreamReader $fs
    $text = $sr.ReadToEnd(); $sr.Close(); $fs.Close()
    return $text
}

function Save-Window($proc, $name) {
    $proc.Refresh()
    $h = $proc.MainWindowHandle
    if ($h -eq 0) { return $false }
    $r = New-Object AFWin+RECT; [AFWin]::GetClientRect($h, [ref]$r) | Out-Null
    $w = $r.R - $r.L; $hgt = $r.B - $r.T
    if ($w -lt 100 -or $hgt -lt 100) { return $false }
    $bmp = New-Object System.Drawing.Bitmap $w, $hgt
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [AFWin]::PrintWindow($h, $hdc, 3) | Out-Null
    $g.ReleaseHdc($hdc)
    $g.Dispose()
    $bmp.Save((Join-Path $out "mp_$name.jpg"), [System.Drawing.Imaging.ImageFormat]::Jpeg)
    $bmp.Dispose()
    return $true
}

# The client starts once the server's mission is up.
$client = $null; $seen = @{}; $done = $false; $text = ''; $clicks = 0
$deadline = (Get-Date).AddSeconds(540)
while ((Get-Date) -lt $deadline -and -not $done) {
    $text = Read-Log $srvProf
    if (-not $client -and $text -match 'Mission script has no main function|\[AFHiveMind\] awake') {
        $client = Start-Process $exe -WorkingDirectory $dayz -PassThru -ArgumentList @(
            '-connect=127.0.0.1', '-port=2302', $modArg, """-profiles=$cliProf""",
            '-window', '-nosplash', '-noPause', '-scriptDebug=true')
        'client started'
    }
    foreach ($m in [regex]::Matches($text, '\[AFHive\] SHOT (\S+)')) {
        $name = $m.Groups[1].Value
        if (-not $seen.ContainsKey($name)) {
            $seen[$name] = $true
            Start-Sleep -Milliseconds 800
            if ($client -and (Save-Window $client $name)) { "captured $name" } else { "could not capture $name" }
        }
    }
    # Between FIRE START and FIRE STOP the left mouse button is clicked in the client window,
    # and only while that window really is in front, so no click can land anywhere else.
    if ($client -and $text -match 'FIRE START' -and $text -notmatch 'FIRE STOP') {
        $client.Refresh(); $h = $client.MainWindowHandle
        if ($h -ne 0) {
            if ([AFWin]::GetForegroundWindow() -ne $h) { [AFWin]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 300 }
            if ([AFWin]::GetForegroundWindow() -eq $h) {
                [AFWin]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 90
                [AFWin]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero); $clicks++
            }
        }
        Start-Sleep -Milliseconds 160
        continue
    }
    if ($text -match 'TOUR DONE') { $done = $true }
    if ($text -match 'ompile error|Can.t compile') { break }
    if ($server.HasExited) { 'server exited'; break }
    if ($client -and $client.HasExited) { 'client exited'; break }
    Start-Sleep -Milliseconds 500
}
'--- server'
($text -split "`r?`n" | Select-String '\[AFHive\]|\[AFHiveMind\]|ompile|SCRIPT\s+\(E\): @').Line
'--- client'
((Read-Log $cliProf) -split "`r?`n" | Select-String '\[AFHiveTest\]|ompile|SCRIPT\s+\(E\): @').Line
"test finished: $done; pictures: $($seen.Count); mouse clicks sent: $clicks"
foreach ($own in @($client, $server)) { if ($own -and -not $own.HasExited) { Stop-Process -Id $own.Id -Force -Confirm:$false } }
