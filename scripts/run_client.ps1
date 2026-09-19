# Run chat_client (forwards all args). Example:
#   .\scripts\run_client.ps1 --host 127.0.0.1 --port 5555
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"

$clientExe = Join-Path $buildDir "chat_client.exe"
if (-not (Test-Path $clientExe)) {
    $clientExe = Join-Path $buildDir "chat_client"
}
if (-not (Test-Path $clientExe)) {
    Write-Error "chat_client not found. Run .\scripts\build.ps1 first."
    exit 1
}

Push-Location $repoRoot
try {
    & $clientExe @args
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
