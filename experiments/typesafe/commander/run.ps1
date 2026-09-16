# Isolated Jev commander benchmark. Does not build or run Carbon hosts.
param(
    [switch]$MeasureOnly,
    [switch]$DryRun,
    [switch]$SkipTests,
    [string]$Phase = "all",
    [int]$Budget = 250
)

$ErrorActionPreference = "Stop"
$Typesafe = Split-Path $PSScriptRoot -Parent
$Python = Join-Path $Typesafe ".venv\Scripts\python.exe"

if (-not $DryRun -and -not $MeasureOnly -and -not $env:TYPESAFE_API_KEY) {
    Write-Error "TYPESAFE_API_KEY is not set. Export it, or pass -DryRun for harness-only output."
}

if (-not (Test-Path $Python)) {
    Write-Host "Creating venv at $Typesafe\.venv"
    python -m venv (Join-Path $Typesafe ".venv")
}

& $Python -m pip install -r (Join-Path $Typesafe "requirements.txt")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $SkipTests) {
    Push-Location $Typesafe
    try {
        & $Python -m unittest discover -s tests -t .
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    } finally {
        Pop-Location
    }
}

$argList = @("-m", "commander.run_commander")
if ($MeasureOnly) {
    $argList += "--measure-only"
} else {
    $argList += @("--phase", $Phase, "--budget", "$Budget")
    if ($DryRun) { $argList += "--dry-run" }
}

Push-Location $Typesafe
try {
    & $Python @argList
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
