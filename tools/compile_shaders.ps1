# Compile GLSL shaders to SPIR-V for the Caustic project
# Usage: run this script from the repo root or call the batch wrapper tools\compile_shaders.bat

$ErrorActionPreference = 'Stop'

# Resolve shader directory relative to this script
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$shaderDir = Resolve-Path (Join-Path $scriptDir "..\Caustic\shaders")

if (-not (Test-Path $shaderDir)) {
 Write-Error "Shaders directory not found: $shaderDir"
 exit1
}

# Locate glslangValidator or glslc
$validators = @()
if ($env:VULKAN_SDK) {
 $glslang = Join-Path $env:VULKAN_SDK "Bin\glslangValidator.exe"
 if (Test-Path $glslang) { $validators += $glslang }
}
# fallback to glslangValidator in PATH
$whichGlslang = (Get-Command glslangValidator -ErrorAction SilentlyContinue)
if ($whichGlslang) { $validators += $whichGlslang.Source }
# fallback to glslc (shaderc)
$whichGlslc = (Get-Command glslc -ErrorAction SilentlyContinue)
if ($whichGlslc) { $validators += $whichGlslc.Source }

if ($validators.Count -eq0) {
 Write-Error "No shader compiler found. Install Vulkan SDK (glslangValidator) or shaderc (glslc) and ensure it's on PATH, or set VULKAN_SDK."
 exit1
}

$compiler = $validators[0]
Write-Host "Using shader compiler: $compiler"

# Include path for #include "common.glsl"
$includeArg = "-I" + " `"$shaderDir`""

$shaders = Get-ChildItem -Path $shaderDir -Include *.vert,*.frag -File -Recurse
if ($shaders.Count -eq0) {
 Write-Host "No shader files found in $shaderDir"
 exit0
}

$failures = @()
foreach ($s in $shaders) {
 $out = [System.IO.Path]::ChangeExtension($s.FullName, ".spv")
 Write-Host "Compiling $($s.Name) -> $([System.IO.Path]::GetFileName($out))"

 if ($compiler -like "*glslangValidator*") {
 $args = @('-V', '-I', $shaderDir, $s.FullName, '-o', $out)
 $proc = Start-Process -FilePath $compiler -ArgumentList $args -NoNewWindow -Wait -PassThru
 if ($proc.ExitCode -ne0) { $failures += $s.FullName }
 } else {
 # assume glslc-like
 $args = @('-I', $shaderDir, $s.FullName, '-o', $out)
 $proc = Start-Process -FilePath $compiler -ArgumentList $args -NoNewWindow -Wait -PassThru
 if ($proc.ExitCode -ne0) { $failures += $s.FullName }
 }
}

if ($failures.Count -gt0) {
 Write-Error "Failed to compile $($failures.Count) shader(s):`n$($failures -join "`n")"
 exit1
}

Write-Host "All shaders compiled successfully."
exit0
