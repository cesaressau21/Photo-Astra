param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [ValidateRange(1, 64)][int]$Jobs = 8
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$skiaRoot = Join-Path $projectRoot '.deps/skia'
$revision = 'ed427fd003ba3bc6eb4a8ae0337f9cdafc39e5fb'
$python = (Get-Command python -ErrorAction Stop).Source
$git = (Get-Command git -ErrorAction Stop).Source
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsPath) { throw 'Se requieren Visual Studio Build Tools con C++.' }
$ninja = Join-Path $vsPath 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
if (!(Test-Path -LiteralPath $ninja)) { throw 'Faltan las herramientas CMake/Ninja de Visual Studio.' }
& (Join-Path $vsPath 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
if (!$?) { throw 'No se pudo preparar Visual Studio.' }
chcp.com 65001 | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'No se pudo activar UTF-8.' }

if (!(Test-Path -LiteralPath $skiaRoot)) {
    New-Item -ItemType Directory -Path $skiaRoot -Force | Out-Null
    & $git -C $skiaRoot init
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo crear el checkout Skia.' }
    & $git -C $skiaRoot remote add origin https://skia.googlesource.com/skia.git
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo configurar el origen Skia.' }
    & $git -C $skiaRoot fetch --depth 1 origin $revision
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo descargar la revisión Skia.' }
    & $git -C $skiaRoot checkout --detach $revision
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo seleccionar la revisión Skia.' }
}
$actual = & $git -C $skiaRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actual -ne $revision) {
    throw "Skia debe estar en $revision. No se modificará automáticamente un checkout existente."
}
$changes = & $git -C $skiaRoot status --porcelain --untracked-files=no
if ($changes) { throw 'El código fuente de Skia tiene cambios locales; revísalos antes de compilar.' }
Push-Location $skiaRoot
try {
    if (!(Test-Path -LiteralPath 'bin/gn.exe')) {
        & $python bin/fetch-gn
        if ($LASTEXITCODE -ne 0) { throw 'No se pudo descargar el GN fijado por Skia.' }
    }
    $output = if ($Configuration -eq 'Debug') { 'out/photoastra-debug' } else { 'out/photoastra' }
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $arguments = Get-Content -LiteralPath (Join-Path $projectRoot 'cmake/skia-common.gn') -Raw
    $runtime = '/MD'
    if ($Configuration -eq 'Debug') {
        $runtime = '/MDd'
        $arguments = $arguments.Replace('is_debug = false', 'is_debug = true').Replace('is_official_build = true', 'is_official_build = false')
    }
    $vcPath = (Join-Path $vsPath 'VC').Replace('\', '/')
    $arguments += "`ntarget_cpu = `"x64`"`nwin_vc = `"$vcPath`"`nextra_cflags = [`"$runtime`", `"/utf-8`"]`nskia_enable_spirv_validation = false`nskia_enable_gpu_debug_layers = false`n"
    [IO.File]::WriteAllText((Join-Path $skiaRoot "$output/args.gn"), $arguments)
    & .\bin\gn.exe gen $output "--script-executable=$python"
    if ($LASTEXITCODE -ne 0) { throw 'Falló la configuración GN.' }

    # GN assumes English MSVC diagnostics. Detect the actual prefix and inherit it
    # through a wrapper, without editing Skia's sources or generated build graph.
    $outputPath = Join-Path $skiaRoot $output
    $probeSource = Join-Path $outputPath 'photoastra-probe.cpp'
    [IO.File]::WriteAllText((Join-Path $outputPath 'photoastra-probe.h'), "#pragma once`n")
    [IO.File]::WriteAllText($probeSource, "#include `"photoastra-probe.h`"`nint photoastra_probe() { return 0; }`n")
    $probeLines = & cl /nologo /utf-8 /showIncludes /c $probeSource "/Fo$outputPath/photoastra-probe.obj" 2>&1
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo detectar el prefijo de includes de MSVC.' }
    $prefix = $null
    foreach ($line in $probeLines) {
        if ("$line" -match '^(.*?)[A-Za-z]:[\\/].*photoastra-probe\.h\s*$') { $prefix = $Matches[1]; break }
    }
    if (!$prefix) { throw 'No se pudo identificar la salida /showIncludes de MSVC.' }
    $wrapperPath = Join-Path $outputPath 'photoastra.ninja'
    $wrapper = 'msvc_deps_prefix = ' + $prefix.Replace('$', '$$') + "`ninclude build.ninja`n"
    $changed = !(Test-Path -LiteralPath $wrapperPath) -or (Get-Content -LiteralPath $wrapperPath -Raw) -ne $wrapper
    [IO.File]::WriteAllText($wrapperPath, $wrapper)
    if ($changed) {
        & $ninja -C $output -f photoastra.ninja -t clean
        if ($LASTEXITCODE -ne 0) { throw 'No se pudo limpiar la salida generada para actualizar sus dependencias.' }
    }
    & $ninja -C $output -f photoastra.ninja -j $Jobs skia
    if ($LASTEXITCODE -ne 0) { throw 'Falló la compilación Skia. Conserva el registro para investigar el error.' }
    Write-Host "Skia $revision ($Configuration) disponible en $outputPath"
} finally { Pop-Location }
