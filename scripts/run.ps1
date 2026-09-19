# Start one server node in a new console window (used by run_cluster.ps1).
# Example:
#   .\scripts\run.ps1 --node-id node1 --port 5555 --peer-port 5557 --peers 127.0.0.1:5558 --db data/node-1.db
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$runServer = Join-Path $PSScriptRoot "run_server.ps1"

if (-not (Test-Path $runServer)) {
    Write-Error "Missing $runServer"
    exit 1
}

$argList = @("-NoExit", "-File", $runServer) + $args
Start-Process -FilePath "powershell.exe" -WorkingDirectory $repoRoot -ArgumentList $argList | Out-Null
