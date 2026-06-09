param(
  [string]$MakeNsisPath = ""
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$nsi = Join-Path $scriptDir 'LANGamesDeployer_xp.nsi'
$releaseExe = Join-Path $scriptDir '..\build-vs2017-xp32\Release\LANGamesDeployerCpp.exe'

if (-not (Test-Path $nsi)) { Write-Error "NSIS script not found: $nsi"; exit 1 }
if (-not (Test-Path $releaseExe)) { Write-Error "Release EXE not found: $releaseExe"; exit 1 }

if ([string]::IsNullOrWhiteSpace($MakeNsisPath)) {
  $candidates = @(
    'C:\Program Files (x86)\NSIS\makensis.exe',
    'C:\Program Files\NSIS\makensis.exe',
    'C:\Program Files (x86)\NSIS\Bin\makensis.exe',
    'C:\Program Files\NSIS\Bin\makensis.exe'
  )
  foreach ($p in $candidates) { if (Test-Path $p) { $MakeNsisPath = $p; break } }
}

if ([string]::IsNullOrWhiteSpace($MakeNsisPath) -or -not (Test-Path $MakeNsisPath)) {
  Write-Error "makensis.exe not found."
  exit 1
}

$outDir = Join-Path $scriptDir 'output'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Push-Location $scriptDir
& $MakeNsisPath /V2 'LANGamesDeployer_xp.nsi'
$exitCode = $LASTEXITCODE
Pop-Location

if ($exitCode -ne 0) { exit $exitCode }
Write-Host "Installer built: $outDir\LAN_Games_Deployer_Setup_v1.0_xp32.exe"
