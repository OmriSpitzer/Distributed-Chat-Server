# Builds the project and runs all tests
$ErrorActionPreference = "Stop"

# Repo root = parent of this script's folder (tests/)
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"

# Configure (only if not already configured)
if (-not (Test-Path (Join-Path $buildDir "CMakeCache.txt"))) {
    cmake -B $buildDir -S $repoRoot -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# Build
cmake --build $buildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Run tests
ctest --test-dir $buildDir --output-on-failure
exit $LASTEXITCODE