# Builds and runs the chat_server executable (src/server/main.cpp)
$ErrorActionPreference = "Stop"

# Repo root = parent of this script's folder (src/)
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"

# Configure (only if not already configured)
if (-not (Test-Path (Join-Path $buildDir "CMakeCache.txt"))) {
    cmake -B $buildDir -S $repoRoot -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# Build only the server executable and its dependencies
cmake --build $buildDir --target chat_server
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Run the server
$serverExe = Join-Path $buildDir "chat_server.exe"
if (-not (Test-Path $serverExe)) {
    $serverExe = Join-Path $buildDir "chat_server"
}
& $serverExe @args
exit $LASTEXITCODE
