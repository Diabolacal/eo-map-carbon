# Build TrinityAL DX11 and the in-tree TrinityAL tests.

$ErrorActionPreference = "Stop"
$TrinityRoot = "C:\dev\carbon-upstream\trinity"
$TrinityBuild = "$TrinityRoot\.cmake-build-local-dx11-debug"
if (-not (Test-Path $TrinityBuild)) {
    Write-Error "Trinity is not configured. Run scripts\configure-trinity.ps1 first."
}

$ninjaDir = "$TrinityRoot\vendor\github.com\microsoft\vcpkg\downloads\tools\ninja-1.13.2-windows"
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$envBlock = cmd /c "`"$vcvars`" -vcvars_ver=14.16 && set"
foreach ($line in $envBlock) {
    if ($line -match "^(.*?)=(.*)$") {
        Set-Item -Path "Env:$($matches[1])" -Value $matches[2]
    }
}
if (Test-Path "$ninjaDir\ninja.exe") {
    $env:PATH = "$ninjaDir;$env:PATH"
}
$env:CMAKE_BUILD_PARALLEL_LEVEL = "12"

cmake --build $TrinityBuild --config Debug --target TrinityAL_dx11 --parallel 12
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $TrinityBuild --config Debug --target TrinityALTest_dx11 --parallel 12
exit $LASTEXITCODE
