# Packs the Release builds of all SSO apps into a zip for a GitHub release.
#   powershell -ExecutionPolicy Bypass -File ".\make-release.ps1" -Version v1.0
# Result: dist\SmoothSynthOperator-<Version>-Windows-x64.zip

param([Parameter(Mandatory)][string]$Version)

$ErrorActionPreference = 'Stop'

$apps = @(
    @{ App = 'SSO-Kiwi3P';  Target = 'SSO_Kiwi3P';  Name = 'SSO Kiwi-3P'  },
    @{ App = 'SSO-KiwiSix'; Target = 'SSO_KiwiSix'; Name = 'SSO KiwiSix'  },
    @{ App = 'SSO-Hawk800'; Target = 'SSO_Hawk800'; Name = 'SSO Hawk-800' }
)

$name = "SmoothSynthOperator-$Version-Windows-x64"
$dist = Join-Path $PSScriptRoot 'dist'
$stage = Join-Path $dist $name

if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force (Join-Path $stage 'VST3'), (Join-Path $stage 'Standalone') | Out-Null

foreach ($a in $apps) {
    $artefacts = Join-Path $PSScriptRoot "build\apps\$($a.App)\$($a.Target)_artefacts\Release"
    Copy-Item -Recurse (Join-Path $artefacts "VST3\$($a.Name).vst3") (Join-Path $stage 'VST3')
    Copy-Item (Join-Path $artefacts "Standalone\$($a.Name).exe") (Join-Path $stage 'Standalone')
}

Copy-Item (Join-Path $PSScriptRoot 'docs\release\INSTALL.txt') $stage
Copy-Item (Join-Path $PSScriptRoot 'LICENSE') (Join-Path $stage 'LICENSE.txt')

$zip = Join-Path $dist "$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path $stage -DestinationPath $zip
Write-Host "Packed $zip" -ForegroundColor Green
