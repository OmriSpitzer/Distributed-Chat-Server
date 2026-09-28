# Configure (if needed), build the Catch2 suite, and run it with CTest.
# Extra arguments are forwarded to ctest.
# Examples:
#   .\scripts\run_test.ps1
#   .\scripts\run_test.ps1 -R "Client login"
#   .\scripts\run_test.ps1 -E "slow"
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"

Push-Location $repoRoot
try {
    if (-not (Test-Path (Join-Path $buildDir "CMakeCache.txt"))) {
        cmake -B $buildDir -S $repoRoot -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    cmake --build $buildDir
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    ctest --test-dir $buildDir --output-on-failure @args
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
