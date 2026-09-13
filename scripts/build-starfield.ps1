# Configure and build the TrinityAL synthetic starfield host.

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path "$PSScriptRoot\.."
$TrinityRoot = "C:\dev\carbon-upstream\trinity"
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
$env:WindowsSDKVersion = "10.0.17763.0\"

Push-Location $RepoRoot
try {
    cmake --preset triangle-debug
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    cmake --build --preset starfield-debug --parallel 12
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
