# Isolated TypeSafe Jev benchmark. Does not build or run Carbon hosts.
param(
    [int]$Runs = 5,
    [switch]$MeasureOnly,
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
$Here = $PSScriptRoot
$Python = Join-Path $Here ".venv\Scripts\python.exe"

if (-not $MeasureOnly -and -not $env:TYPESAFE_API_KEY) {
    Write-Error "TYPESAFE_API_KEY is not set. Export it in this shell before running."
}

if (-not (Test-Path $Python)) {
    Write-Host "Creating venv at $Here\.venv"
    python -m venv (Join-Path $Here ".venv")
}

& $Python -m pip install -r (Join-Path $Here "requirements.txt")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $SkipTests) {
    Push-Location $Here
    try {
        & $Python -m unittest discover -s tests -t .
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    } finally {
        Pop-Location
    }
}

$argsList = @((Join-Path $Here "run_benchmark.py"))
if ($MeasureOnly) {
    $argsList += "--measure-only"
} else {
    $argsList += @("--runs", "$Runs")
}

& $Python @argsList
exit $LASTEXITCODE
