param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Se requieren Visual Studio Build Tools con C++.' }
$vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsPath) { throw 'No se encontró el compilador C++ de Visual Studio.' }
& (Join-Path $vsPath 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
if (!$?) { throw 'No se pudo preparar Visual Studio.' }
chcp.com 65001 | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'No se pudo activar UTF-8.' }
$cmakeRoot = Join-Path $vsPath 'Common7/IDE/CommonExtensions/Microsoft/CMake'
$cmake = Join-Path $cmakeRoot 'CMake/bin/cmake.exe'
$ctest = Join-Path $cmakeRoot 'CMake/bin/ctest.exe'
$ninja = Join-Path $cmakeRoot 'Ninja/ninja.exe'
foreach ($tool in @($cmake, $ctest, $ninja)) {
    if (!(Test-Path -LiteralPath $tool)) { throw "Falta la herramienta: $tool. Instala CMake tools for Windows en Visual Studio." }
}
$preset = "core-$($Configuration.ToLowerInvariant())"
Push-Location -LiteralPath $projectRoot
try {
    & $cmake --preset $preset "-DCMAKE_MAKE_PROGRAM=$ninja"
    if ($LASTEXITCODE -ne 0) { throw 'Falló la configuración del núcleo.' }
    & $cmake --build --preset $preset --parallel 2
    if ($LASTEXITCODE -ne 0) { throw 'Falló la compilación del núcleo.' }
    & $ctest --preset $preset
    if ($LASTEXITCODE -ne 0) { throw 'Fallaron las pruebas del núcleo.' }
} finally {
    Pop-Location
}
