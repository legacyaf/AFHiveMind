# Starts the offline HiveMind test (mission AFHive, see its init.c), follows the script log
# until the mission prints TOUR DONE, and saves a picture of the DayZ window at each SHOT.
# Once, it brings the DayZ window to the front and presses Esc in it (the admin controls
# test), and drags that window with the mouse: leave the mouse and keyboard alone while it runs.
# Pictures go to testmission\captures. Needs Steam running and these Workshop mods
# installed: CF, Dabs Framework, DayZ-Expansion-Core, DayZ-Expansion-AI.
#   run_test.ps1          start with whatever brain.json the test profile already holds
#   run_test.ps1 -Fresh   delete the test profile's AFHiveMind folder first (empty brain)
# Ends with TEST PASSED (exit code 0) or TEST FAILED (exit code 1).
#   run_test.ps1 -Webhook <url>   also post to a real Discord webhook (the URL is put only in the
#                                 git-ignored test profile, and removed when the test ends)
param([switch]$Fresh, [string]$Webhook = '')
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
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, int dx, int dy, uint data, UIntPtr extra);
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
$webhookFile = Join-Path $profiles 'AFHiveMind\test_webhook.txt'
Remove-Item $webhookFile -ErrorAction SilentlyContinue
if ($Webhook) {
    New-Item -ItemType Directory -Force (Split-Path $webhookFile) | Out-Null
    [IO.File]::WriteAllText($webhookFile, $Webhook.Trim())
}
# A stand-in for Discord: the mission points the webhook at 127.0.0.1:8765, and every post that
# arrives is saved to captures\webhook.txt (only this machine can reach it).
$hookFile = Join-Path $out 'webhook.txt'
Remove-Item $hookFile -ErrorAction SilentlyContinue
$hook = Start-Job -ArgumentList $hookFile -ScriptBlock {
    param($file)
    $l = New-Object System.Net.Sockets.TcpListener([Net.IPAddress]::Loopback, 8765)
    $l.Start()
    $until = (Get-Date).AddSeconds(480)
    while ((Get-Date) -lt $until) {
        if (-not $l.Pending()) { Start-Sleep -Milliseconds 100; continue }
        $c = $l.AcceptTcpClient(); $c.ReceiveTimeout = 3000
        $s = $c.GetStream(); $buf = New-Object byte[] 65536; $got = 0; $text = ''
        try {
            do {
                $n = $s.Read($buf, $got, $buf.Length - $got); $got += $n
                $text = [Text.Encoding]::UTF8.GetString($buf, 0, $got)
                $head = $text.IndexOf("`r`n`r`n")
                $len = 0; if ($text -match '(?i)content-length:\s*(\d+)') { $len = [int]$Matches[1] }
            } while ($n -gt 0 -and ($head -lt 0 -or $got - $head - 4 -lt $len))
        } catch {}
        $resp = [Text.Encoding]::ASCII.GetBytes("HTTP/1.1 200 OK`r`nContent-Type: application/json`r`nContent-Length: 2`r`nConnection: close`r`n`r`n{}")
        try { $s.Write($resp, 0, $resp.Length) } catch {}
        $c.Close()
        $body = $text; $head = $text.IndexOf("`r`n`r`n"); if ($head -ge 0) { $body = $text.Substring($head + 4) }
        $first = ($text -split "`r`n")[0]
        Add-Content -LiteralPath $file -Value "$first | $body" -Encoding UTF8
    }
    $l.Stop()
}
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
        # The mission asks for a real Esc press to test closing the admin controls. It is sent
        # only while the DayZ window is in front, so it can never land in another program.
        if ($text -match '\[AFHive\] PRESS ESC' -and -not $seen.ContainsKey('#esc')) {
            $seen['#esc'] = $true
            $game.Refresh(); $h = $game.MainWindowHandle
            if ($h -ne 0 -and [AFWin]::GetForegroundWindow() -ne $h) { [AFWin]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 400 }
            if ($h -ne 0 -and [AFWin]::GetForegroundWindow() -eq $h) {
                [AFWin]::keybd_event(0x1B, 0x01, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 80
                [AFWin]::keybd_event(0x1B, 0x01, 2, [UIntPtr]::Zero)
                'pressed Esc in the game window'
            } else { 'could not press Esc: the DayZ window would not come to the front' }
        }
        # The mission asks for a mouse drag of the admin controls window (client pixels, then the
        # distance to move). Like the Esc press, only while the DayZ window is in front.
        $drag = [regex]::Match($text, '\[AFHive\] PRESS DRAG (-?\d+) (-?\d+) (-?\d+) (-?\d+)')
        if ($drag.Success -and -not $seen.ContainsKey('#drag')) {
            $seen['#drag'] = $true
            $game.Refresh(); $h = $game.MainWindowHandle
            if ($h -ne 0 -and [AFWin]::GetForegroundWindow() -ne $h) { [AFWin]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 400 }
            if ($h -ne 0 -and [AFWin]::GetForegroundWindow() -eq $h) {
                $pt = New-Object AFWin+POINT; $pt.X = [int]$drag.Groups[1].Value; $pt.Y = [int]$drag.Groups[2].Value
                [AFWin]::ClientToScreen($h, [ref]$pt) | Out-Null
                $dx = [int]$drag.Groups[3].Value; $dy = [int]$drag.Groups[4].Value
                [AFWin]::SetCursorPos($pt.X, $pt.Y) | Out-Null; Start-Sleep -Milliseconds 200
                [AFWin]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 150
                for ($s = 1; $s -le 12; $s++) {
                    [AFWin]::SetCursorPos($pt.X + [int]($dx * $s / 12), $pt.Y + [int]($dy * $s / 12)) | Out-Null
                    Start-Sleep -Milliseconds 40
                }
                Start-Sleep -Milliseconds 150
                [AFWin]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
                'dragged the admin controls in the game window'
            } else { 'could not drag: the DayZ window would not come to the front' }
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
Stop-Job $hook -ErrorAction SilentlyContinue; Remove-Job $hook -Force -ErrorAction SilentlyContinue
Remove-Item $webhookFile -ErrorAction SilentlyContinue
if (Test-Path $hookFile) { Get-Content $hookFile | ForEach-Object { "webhook received: $_" } } else { 'webhook received: nothing' }

# A clear verdict and exit code, so tools\release.ps1 can stop on a failure.
$failed = @($lines | Where-Object { $_ -cmatch 'FAILED' -or $_ -match 'ompile error|Can.t compile' })
# Script errors: each one is read with the lines after it (message, class, function, call
# stack). It fails the test only when it points into this mod's own scripts (a path under
# AFHiveMind/Scripts, or an AFHM_ function). Errors from the game or other mods are grouped
# by class and function and shown as warnings.
$all = $text -split "`r?`n"
$others = @{}
for ($i = 0; $i -lt $all.Count; $i++) {
    if ($all[$i] -notmatch 'SCRIPT\s*\(E\).*(Virtual Machine Exception|-- Stack trace --)') { continue }
    $last = [Math]::Min($i + 40, $all.Count - 1)
    $block = @($all[$i..$last])
    $stop = 0
    for ($j = 1; $j -lt $block.Count; $j++) { if ($block[$j] -match '-----------------') { $stop = $j; break } }
    if ($stop -gt 0) { $block = $block[0..$stop] }
    $body = $block -join "`n"
    if ($body -match '(?i)afhivemind[/\\]scripts[/\\]|\bAFHM_\w+\(') {
        $failed += ($block -join "`n").Trim()
    } else {
        $where = (@($block | Select-String 'Class:|Function:|Reason:|\(\) scripts' | Select-Object -First 3).Line | ForEach-Object { ($_ -replace '^.*SCRIPT\s*(\(E\))?\s*:\s*', '').Trim() }) -join ' | '
        if (-not $where) { $where = 'no details in the log' }
        $others[$where] = 1 + [int]$others[$where]
    }
    $i += [Math]::Max($block.Count - 1, 0)
}
foreach ($k in $others.Keys) { "warning, script error not from this mod (x$($others[$k])): $k" }
if ($failed.Count -gt 0) { 'Failures:'; $failed }
if (-not $done -or $failed.Count -gt 0) {
    if (-not $done) { 'the mission did not reach TOUR DONE (script error, crash or timeout)' }
    'TEST FAILED'
    exit 1
}
'TEST PASSED'
exit 0
