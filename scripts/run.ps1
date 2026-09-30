# Start one server node and one client in new console windows (used by run_cluster.ps1).
# The client connects to this node's --port (default 5555) on 127.0.0.1.
# --test, if present, is forwarded to both processes.
# Example:
#   .\scripts\run.ps1 --node-id node1 --port 5555 --peer-port 5557 --peers 127.0.0.1:5558 --db data/node-1.db
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$runServer = Join-Path $PSScriptRoot "run_server.ps1"
$runClient = Join-Path $PSScriptRoot "run_client.ps1"

if (-not (Test-Path $runServer)) {
    Write-Error "Missing $runServer"
    exit 1
}
if (-not (Test-Path $runClient)) {
    Write-Error "Missing $runClient"
    exit 1
}

$port = "5555"
$clientExtra = @()
for ($i = 0; $i -lt $args.Count; $i++) {
    if ($args[$i] -eq "--port" -and ($i + 1) -lt $args.Count) {
        $port = $args[$i + 1]
    }
    elseif ($args[$i] -eq "--test") {
        $clientExtra += "--test"
    }
}

$serverArgs = @("-NoExit", "-File", $runServer) + $args
Start-Process -FilePath "powershell.exe" -WorkingDirectory $repoRoot -ArgumentList $serverArgs | Out-Null

# Give the server a moment to bind before the client connects
Start-Sleep -Seconds 1

$clientArgs = @("-NoExit", "-File", $runClient, "--host", "127.0.0.1", "--port", $port) + $clientExtra
Start-Process -FilePath "powershell.exe" -WorkingDirectory $repoRoot -ArgumentList $clientArgs | Out-Null
