# Launch the synthetic starfield host. Use -Smoke for a timed auto-exit.

param(
    [switch]$Smoke
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path "$PSScriptRoot\.."
$binDir = Join-Path $RepoRoot ".cmake-build-triangle-debug\bin"
$ours = Join-Path $binDir "eo-map-carbon-starfield.exe"
$upstreamTestDir = "C:\dev\carbon-upstream\trinity\.cmake-build-local-dx11-debug\carbon\autobuild\TrinityALTest\Windows\x64\v141"

if (-not (Test-Path $ours)) {
    Write-Error "Starfield exe is not built yet. Run .\scripts\build-starfield.ps1 first."
}

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
