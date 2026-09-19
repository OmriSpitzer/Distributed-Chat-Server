# Configure (if needed) and build chat_server + chat_client
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"

Push-Location $repoRoot
try {
    if (-not (Test-Path (Join-Path $buildDir "CMakeCache.txt"))) {
        cmake -B $buildDir -S $repoRoot -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    cmake --build $buildDir --target chat_server chat_client
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
    Pop-Location
}
