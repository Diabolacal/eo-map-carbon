# Launch the New Eden systems + stargate host. Use -Smoke for a timed auto-exit.

param(
    [switch]$Smoke,
    [switch]$BloomOff,
    [switch]$Points
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path "$PSScriptRoot\.."
$binDir = Join-Path $RepoRoot ".cmake-build-triangle-debug\bin"
$ours = Join-Path $binDir "eo-map-carbon-neweden.exe"
$catalog = Join-Path $binDir "new_eden_systems.bin"
$gates = Join-Path $binDir "new_eden_stargates.bin"
$visuals = Join-Path $binDir "new_eden_star_visuals.bin"
$upstreamTestDir = "C:\dev\carbon-upstream\trinity\.cmake-build-local-dx11-debug\carbon\autobuild\TrinityALTest\Windows\x64\v141"

if (-not (Test-Path $ours)) {
    Write-Error "New Eden exe is not built yet. Run .\scripts\build-neweden.ps1 first."
}

if (-not (Test-Path $catalog)) {
    Write-Error "new_eden_systems.bin is missing from $binDir. Rebuild with .\scripts\build-neweden.ps1."
}

if (-not (Test-Path $gates)) {
    Write-Error "new_eden_stargates.bin is missing from $binDir. Rebuild with .\scripts\build-neweden.ps1."
}

if (-not (Test-Path $visuals)) {
    Write-Error "new_eden_star_visuals.bin is missing from $binDir. Rebuild with .\scripts\build-neweden.ps1."
}

if (-not (Test-Path (Join-Path $binDir "CcpCore_debug.dll")) -and (Test-Path $upstreamTestDir)) {
    Write-Host "Copying TrinityALTest runtime DLLs into $binDir"
    Copy-Item (Join-Path $upstreamTestDir "*.dll") $binDir -ErrorAction SilentlyContinue
}

Push-Location $binDir
try {
    $cliArgs = @()
    if ($Smoke) { $cliArgs += "--smoke" }
    if ($BloomOff) { $cliArgs += "--bloom-off" }
    if ($Points) { $cliArgs += "--points" }
    Write-Host "Launching $ours $($cliArgs -join ' ')"
    if ($cliArgs.Count -gt 0) {
        & $ours @cliArgs
    }
    else {
        & $ours
    }
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
