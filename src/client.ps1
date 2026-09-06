# Builds and runs the chat_client executable (src/client/main.cpp)
$ErrorActionPreference = "Stop"

# Repo root = parent of this script's folder (src/)
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"

# Configure (only if not already configured)
if (-not (Test-Path (Join-Path $buildDir "CMakeCache.txt"))) {
    cmake -B $buildDir -S $repoRoot -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# Build only the client executable and its dependencies
cmake --build $buildDir --target chat_client
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Run the client
$clientExe = Join-Path $buildDir "chat_client.exe"
if (-not (Test-Path $clientExe)) {
    $clientExe = Join-Path $buildDir "chat_client"
}
& $clientExe
exit $LASTEXITCODE
