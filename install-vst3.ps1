# Copies the Release builds of all SSO plugins into the system VST3 folder.
# Run from an administrator PowerShell:
#   powershell -ExecutionPolicy Bypass -File ".\install-vst3.ps1"

$ErrorActionPreference = 'Stop'

$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "Please run this script from an administrator PowerShell." -ForegroundColor Red
    exit 1
}

$target = Join-Path $env:CommonProgramFiles 'VST3'
$plugins = @(
    @{ App = 'SSO-Kiwi3P';  Target = 'SSO_Kiwi3P';  Name = 'SSO Kiwi-3P'  },
    @{ App = 'SSO-KiwiSix'; Target = 'SSO_KiwiSix'; Name = 'SSO KiwiSix'  },
    @{ App = 'SSO-Hawk800'; Target = 'SSO_Hawk800'; Name = 'SSO Hawk-800' }
)

foreach ($p in $plugins) {
    $bundle = Join-Path $PSScriptRoot "build\apps\$($p.App)\$($p.Target)_artefacts\Release\VST3\$($p.Name).vst3"

    if (-not (Test-Path $bundle)) {
        Write-Host "Not built: $bundle" -ForegroundColor Yellow
        continue
    }

    Copy-Item -Recurse -Force $bundle $target

    # The bundle is a folder whose date does not change - show the date of the plugin file inside
    $binary = Join-Path $target "$($p.Name).vst3\Contents\x86_64-win\$($p.Name).vst3"
    Write-Host ("{0,-14} installed, built {1}" -f $p.Name, (Get-Item $binary).LastWriteTime) -ForegroundColor Green
}
