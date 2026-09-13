# Launch the triangle host. Use -Smoke for a 30-frame headless-ish exit.

param(
    [switch]$Smoke
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path "$PSScriptRoot\.."
$binDir = Join-Path $RepoRoot ".cmake-build-triangle-debug\bin"
$ours = Join-Path $binDir "eo-map-carbon-triangle.exe"
$upstreamTestDir = "C:\dev\carbon-upstream\trinity\.cmake-build-local-dx11-debug\carbon\autobuild\TrinityALTest\Windows\x64\v141"
$upstreamTest = Join-Path $upstreamTestDir "TrinityALTest_dx11_debug.exe"

if (Test-Path $ours) {
    if (-not (Test-Path (Join-Path $binDir "CcpCore_debug.dll")) -and (Test-Path $upstreamTestDir)) {
        Write-Host "Copying TrinityALTest runtime DLLs into $binDir"
        Copy-Item (Join-Path $upstreamTestDir "*.dll") $binDir -ErrorAction SilentlyContinue
    }
    Push-Location $binDir
    try {
        Write-Host "Launching $ours$(if ($Smoke) { ' --smoke' })"
        if ($Smoke) {
            & $ours --smoke
        }
        else {
            & $ours
        }
        exit $LASTEXITCODE
    }
    finally {
        Pop-Location
    }
}

if (Test-Path $upstreamTest) {
    Write-Host "Our exe is not built yet. Launching TrinityALTest_dx11 CanRenderASingleTriangle --interactive"
    Push-Location $upstreamTestDir
    try {
        & $upstreamTest --gtest_filter=Rendering.CanRenderASingleTriangle --interactive
        exit $LASTEXITCODE
    }
    finally {
        Pop-Location
    }
}

Write-Error "Nothing to run. Configure and build Trinity, then the triangle app."
