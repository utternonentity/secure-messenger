Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

param(
    [string]$BuildDir = $(Join-Path (Split-Path -Parent $PSScriptRoot) 'build/client-qt'),
    [string]$Prefix = $(Join-Path (Split-Path -Parent $PSScriptRoot) 'dist/client'),
    [string]$Generator = ''
)

function Show-Usage {
    @'
Usage: install_client.ps1 [-BuildDir path] [-Prefix path] [-Generator name]

Options:
  -BuildDir  Build directory to use (default: build/client-qt)
  -Prefix    Install prefix (default: dist/client)
  -Generator CMake generator (default: Ninja if available, otherwise CMake default)
'@ | Write-Output
}

if ($args -contains '-h' -or $args -contains '--help') {
    Show-Usage
    exit 0
}

$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$Prefix = [IO.Path]::GetFullPath($Prefix)

if (-not $Generator) {
    if (Get-Command ninja -ErrorAction SilentlyContinue) {
        $Generator = 'Ninja'
    }
}

$configureArgs = @('-S', (Join-Path $Root 'client-qt'), '-B', $BuildDir)
if ($Generator) {
    $configureArgs += "-G$Generator"
}

cmake @configureArgs
cmake --build $BuildDir
cmake --install $BuildDir --prefix $Prefix

Write-Output "✅ sm_client installed to: $Prefix"
