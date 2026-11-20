# Packs source\AFHiveMind into @AFHiveMind\Addons\AFHiveMind.pbo and signs it.
# Close any DayZ that has the mod loaded first: it keeps the PBO locked.
#   powershell -ExecutionPolicy Bypass -File tools\build.ps1
$ErrorActionPreference = 'Stop'
$repo   = Split-Path $PSScriptRoot -Parent
$tools  = 'C:\Program Files (x86)\Steam\steamapps\common\DayZ Tools'
$addons = Join-Path $repo '@AFHiveMind\Addons'
$temp   = Join-Path $env:TEMP 'AFHiveMind_build'
$pbo    = Join-Path $addons 'AFHiveMind.pbo'

if (Test-Path $pbo) {
    try { $lock = [IO.File]::Open($pbo, 'Open', 'ReadWrite', 'None'); $lock.Close() }
    catch { throw 'AFHiveMind.pbo is in use; close the DayZ that has the mod loaded first.' }
}
New-Item -ItemType Directory -Force $temp, $addons | Out-Null

# Scripts only: nothing to binarize, so the files are packed as they are.
$log = Join-Path $temp 'addonbuilder.log'
$p = Start-Process (Join-Path $tools 'Bin\AddonBuilder\AddonBuilder.exe') -ArgumentList @(
    "`"$(Join-Path $repo 'source\AFHiveMind')`"", "`"$addons`"", '-clear', '-packonly', '-prefix=AFHiveMind',
    "-project=`"$(Join-Path $repo 'source')`"", "-temp=`"$temp`"", "-toolsDirectory=`"$tools`""
) -RedirectStandardOutput $log -NoNewWindow -PassThru -Wait
Get-Content $log | Select-String 'Build |ERROR' | ForEach-Object Line
if ($p.ExitCode -ne 0 -or -not (Test-Path $pbo)) { throw "Addon Builder failed; see $log" }

# Re-sign: a rebuilt PBO no longer matches its old signature. The private key stays in
# keys\ (not in git); without it the stale signature is removed and the PBO is left unsigned.
$key = Join-Path $repo 'keys\AFHiveMind.biprivatekey'
Get-ChildItem $addons -Filter '*.bisign' | Remove-Item -Confirm:$false
if (Test-Path $key) {
    & (Join-Path $tools 'Bin\DsUtils\DSSignFile.exe') $key $pbo
    if ($LASTEXITCODE -ne 0) { throw 'Signing failed.' }
    New-Item -ItemType Directory -Force (Join-Path $repo '@AFHiveMind\Keys') | Out-Null
    Copy-Item (Join-Path $repo 'keys\AFHiveMind.bikey') (Join-Path $repo '@AFHiveMind\Keys\') -Force
} else { Write-Warning 'keys\AFHiveMind.biprivatekey not found; the PBO is unsigned.' }
Get-ChildItem $addons | Select-Object Name, Length, LastWriteTime
