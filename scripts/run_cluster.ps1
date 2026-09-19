param(
    [switch]$Test
)

# Build, then start a 2-node cluster (2 servers + 1 client per node).
# Topology matches README "Two-node cluster":
#   node1  client :5555  gossip :5557  peers 127.0.0.1:5558
#   node2  client :5556  gossip :5558  peers 127.0.0.1:5557
#
# Usage:
#   .\scripts\run_cluster.ps1
#   .\scripts\run_cluster.ps1 -Test   # console UIs (--test)
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildScript = Join-Path $PSScriptRoot "build.ps1"
$runScript = Join-Path $PSScriptRoot "run.ps1"
$runClient = Join-Path $PSScriptRoot "run_client.ps1"

& $buildScript
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$dataDir = Join-Path $repoRoot "data"
if (-not (Test-Path $dataDir)) {
    New-Item -ItemType Directory -Path $dataDir | Out-Null
}

$extra = @()
if ($Test) {
    $extra = @("--test")
}

# Two server processes = two gossip nodes
$node1Args = @("--node-id", "node1", "--port", "5555", "--peer-port", "5557",
    "--peers", "127.0.0.1:5558", "--db", "data/node-1.db") + $extra
$node2Args = @("--node-id", "node2", "--port", "5556", "--peer-port", "5558",
    "--peers", "127.0.0.1:5557", "--db", "data/node-2.db") + $extra

& $runScript @node1Args
& $runScript @node2Args

# Give servers a moment to bind before clients connect
Start-Sleep -Seconds 1

function Start-ClientWindow([string[]]$ClientArgs) {
    $argList = @("-NoExit", "-File", $runClient) + $ClientArgs
    Start-Process -FilePath "powershell.exe" -WorkingDirectory $repoRoot -ArgumentList $argList | Out-Null
}

Start-ClientWindow (@("--host", "127.0.0.1", "--port", "5555") + $extra)
Start-ClientWindow (@("--host", "127.0.0.1", "--port", "5556") + $extra)

Write-Host ""
Write-Host "Started 2-node cluster with 2 clients:"
Write-Host "  node1  server :5555 / gossip :5557  + client -> 127.0.0.1:5555"
Write-Host "  node2  server :5556 / gossip :5558  + client -> 127.0.0.1:5556"
