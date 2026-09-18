param(
    [string]$QtRoot,
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [switch]$Run,
    [switch]$Fresh
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (!$QtRoot) { $QtRoot = Join-Path $projectRoot '.deps/Qt/6.8.3/msvc2022_64' }
$QtRoot = (Resolve-Path -LiteralPath $QtRoot).Path
if (!(Test-Path -LiteralPath (Join-Path $QtRoot 'lib/cmake/Qt6/Qt6Config.cmake'))) {
    throw 'QtRoot debe apuntar a una instalación Qt 6 para MSVC x64.'
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Se requieren Visual Studio Build Tools con C++.' }
$vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsPath) { throw 'No se encontró el compilador C++ de Visual Studio.' }
& (Join-Path $vsPath 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
if (!$?) { throw 'No se pudo preparar el entorno de Visual Studio.' }
# Keep CMake's detection and Ninja's MSVC dependency parsing on the same code page.
chcp.com 65001 | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'No se pudo activar UTF-8 en la consola.' }
$cmake = Join-Path $vsPath 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
$ninja = Join-Path $vsPath 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
foreach ($tool in @($cmake, $ctest, $ninja)) {
    if (!(Test-Path -LiteralPath $tool)) { throw "Falta la herramienta: $tool. Instala CMake tools for Windows en Visual Studio." }
}
$buildDir = Join-Path $projectRoot "build/$($Configuration.ToLowerInvariant())"
$configureOptions = @()
if ($Fresh) { $configureOptions += '--fresh' }
& $cmake @configureOptions -S $projectRoot -B $buildDir -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_BUILD_TYPE=$Configuration" "-DCMAKE_PREFIX_PATH=$QtRoot" -DPHOTO_ASTRA_WARNINGS_AS_ERRORS=ON
if ($LASTEXITCODE -ne 0) { throw 'Falló la configuración CMake.' }
& $cmake --build $buildDir --parallel
if ($LASTEXITCODE -ne 0) { throw 'Falló la compilación.' }
& $ctest --test-dir $buildDir --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Fallaron las pruebas.' }
if ($Run) {
    $previousPath = $env:PATH
    try {
        $env:PATH = (Join-Path $QtRoot 'bin') + [IO.Path]::PathSeparator + $previousPath
        Start-Process -FilePath (Join-Path $buildDir 'photo_astra.exe')
    } finally { $env:PATH = $previousPath }
}
