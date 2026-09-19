# Run chat_server (forwards all args). Example:
#   .\scripts\run_server.ps1 --node-id node1 --port 5555 --peer-port 5557 --peers 127.0.0.1:5558 --db data/node-1.db
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"

$serverExe = Join-Path $buildDir "chat_server.exe"
if (-not (Test-Path $serverExe)) {
    $serverExe = Join-Path $buildDir "chat_server"
}
if (-not (Test-Path $serverExe)) {
    Write-Error "chat_server not found. Run .\scripts\build.ps1 first."
    exit 1
}

$dataDir = Join-Path $repoRoot "data"
if (-not (Test-Path $dataDir)) {
    New-Item -ItemType Directory -Path $dataDir | Out-Null
}

Push-Location $repoRoot
try {
    & $serverExe @args
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
