# Configure the public carbonengine/trinity checkout for a DX11 TrinityAL build.
# Trinity itself is NOT a submodule of this repo.

$ErrorActionPreference = "Stop"

$TrinityRoot = "C:\dev\carbon-upstream\trinity"
if (-not (Test-Path "$TrinityRoot\CMakeLists.txt")) {
    Write-Error "Clone https://github.com/carbonengine/trinity into $TrinityRoot first (HTTPS, --recurse-submodules)."
}

# Rewrite SSH GitHub URLs. Many Carbon vcpkg portfiles still use git@github.com
# even though the repositories are public. This is a git config, not a Carbon fork.
git config --global url.https://github.com/.insteadOf git@github.com:

$env:GIT_CONFIG_COUNT = "1"
$env:GIT_CONFIG_KEY_0 = "url.https://github.com/.insteadOf"
$env:GIT_CONFIG_VALUE_0 = "git@github.com:"
$env:VCPKG_OVERLAY_PORTS = (Resolve-Path "$PSScriptRoot\..\overlays\vcpkg").Path
$env:VCPKG_MAX_CONCURRENCY = "12"
$env:PATH_TO_VCPKG_ROOT = "$TrinityRoot\vendor\github.com\microsoft\vcpkg"
$env:WindowsSDKVersion = "10.0.17763.0\"
$env:UCRTVersion = "10.0.17763.0"

$ninjaDir = "$TrinityRoot\vendor\github.com\microsoft\vcpkg\downloads\tools\ninja-1.13.2-windows"
if (Test-Path "$ninjaDir\ninja.exe") {
    $env:PATH = "$ninjaDir;$env:PATH"
}

$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    Write-Error "vcvars64.bat not found at $vcvars"
}

Write-Host "GIT insteadOf: git@github.com: -> https://github.com/"
Write-Host "VCPKG_OVERLAY_PORTS=$env:VCPKG_OVERLAY_PORTS"
Write-Host "Importing v141 x64 environment from vcvars64.bat -vcvars_ver=14.16"

$envBlock = cmd /c "`"$vcvars`" -vcvars_ver=14.16 && set"
foreach ($line in $envBlock) {
    if ($line -match "^(.*?)=(.*)$") {
        Set-Item -Path "Env:$($matches[1])" -Value $matches[2]
    }
}

if (Test-Path "$ninjaDir\ninja.exe") {
    $env:PATH = "$ninjaDir;$env:PATH"
}

$presetSrc = Resolve-Path "$PSScriptRoot\..\cmake\trinity-CMakeUserPresets.json"
Copy-Item $presetSrc "$TrinityRoot\CMakeUserPresets.json" -Force
Write-Host "Wrote $TrinityRoot\CMakeUserPresets.json from experiment template (gitignored by Trinity)."

Push-Location $TrinityRoot
try {
    cmake --preset local-dx11-debug
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
