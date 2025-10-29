<#
Build-Shaders.ps1

Inlines `common.glsl` into each shader that uses `#include "common.glsl"`,
compiles vertex/fragment GLSL to SPIR-V using `glslangValidator`, and copies
the generated .spv files into the runtime `bin\<Config>-windows-x86_64\Caustic\shaders` folder.

Usage:
  .\scripts\Build-Shaders.ps1 [-BuildConfig Debug] [-Platform x64] [-OutDir <path>]

Requirements:
  - glslangValidator must be on PATH (or available in the environment).

#>

param(
    [string]$BuildConfig = "Debug",
    [string]$Platform = "x64",
    [string]$OutDir = ""
)

set-StrictMode -Version Latest

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot = Resolve-Path (Join-Path $scriptDir "..")
$shaderDir = Join-Path $repoRoot "Caustic\shaders"

if (-not (Test-Path $shaderDir)) {
    Write-Error "Shader directory not found: $shaderDir"
    exit 2
}

$glslang = Get-Command glslangValidator -ErrorAction SilentlyContinue
if (-not $glslang) {
    Write-Error "glslangValidator not found on PATH. Please install it or add it to PATH."
    exit 3
}

if ([string]::IsNullOrEmpty($OutDir)) {
    $outRuntime = Join-Path $repoRoot "bin\$BuildConfig-windows-x86_64\Caustic\shaders"
} else {
    $outRuntime = $OutDir
}

Write-Host "Shader dir: $shaderDir"
Write-Host "Output runtime shader dir: $outRuntime"

New-Item -ItemType Directory -Force -Path $outRuntime | Out-Null

$common = Join-Path $shaderDir "common.glsl"
if (-not (Test-Path $common)) {
    Write-Error "common.glsl not found in $shaderDir"
    exit 4
}

$shaders = @(Get-ChildItem -Path $shaderDir -File | Where-Object { $_.Extension -in @('.vert', '.frag') })
if ($shaders.Count -eq 0) {
    Write-Warning "No .vert or .frag files found in $shaderDir"
}

foreach ($s in $shaders) {
    $inlined = Join-Path $shaderDir ("{0}.inl{1}" -f $s.BaseName, $s.Extension)
    Write-Host "Inlining $($s.Name) -> $([IO.Path]::GetFileName($inlined))"

    try {
    # Build an inlined shader with the original shader's #version first,
    # then the contents of common.glsl (without any #version lines),
    # then the shader body (without its #version or #include lines).
    $shaderLinesArray = Get-Content $s.FullName -ErrorAction Stop -Encoding UTF8
    $versionLine = ($shaderLinesArray | Where-Object { $_ -match '^\s*#version' } | Select-Object -First 1)
    if (-not $versionLine) { $versionLine = "#version 450" }

    $commonLines = Get-Content $common | Where-Object { $_ -notmatch '^\s*#version' }

    $shaderBody = $shaderLinesArray | Where-Object { $_ -notmatch '^\s*#version' -and $_ -notmatch '^\s*#include' }

    $outLines = @()
    $outLines += $versionLine
    $outLines += $commonLines
    $outLines += $shaderBody
    $outLines | Set-Content -Path $inlined -Encoding UTF8

        $outSpv = Join-Path $shaderDir ("$($s.Name).spv")
        Write-Host "Compiling $([IO.Path]::GetFileName($inlined)) -> $([IO.Path]::GetFileName($outSpv))"

        & glslangValidator -V $inlined -o $outSpv
        if ($LASTEXITCODE -ne 0) {
            Write-Error "glslangValidator failed for $($s.Name) (exit $LASTEXITCODE)"
            Remove-Item $inlined -ErrorAction SilentlyContinue
            exit $LASTEXITCODE
        }

        Copy-Item $outSpv -Destination $outRuntime -Force
        Write-Host "Copied: $([IO.Path]::GetFileName($outSpv)) -> $outRuntime"

    } finally {
        Remove-Item $inlined -ErrorAction SilentlyContinue
    }
}

Write-Host "Shader build complete."
