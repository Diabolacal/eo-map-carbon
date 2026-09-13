# Launch the New Eden visual lab (sized stars, bloom, live sliders).
# Same host as run-neweden.ps1. Use that script's -Smoke / -BloomOff / -Points flags
# when you need those variants.

$ErrorActionPreference = "Stop"
& "$PSScriptRoot\run-neweden.ps1"
exit $LASTEXITCODE
