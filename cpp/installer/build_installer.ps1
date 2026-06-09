param(
  [string]$IsccPath = ""
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$iss = Join-Path $scriptDir 'LANGamesDeployer_xp.iss'

if (-not (Test-Path $iss)) {
  Write-Error "Installer script not found: $iss"
  exit 1
}

$releaseExe = Join-Path $scriptDir '..\build-vs2017-xp32\Release\LANGamesDeployerCpp.exe'
if (-not (Test-Path $releaseExe)) {
  Write-Error "Release EXE not found: $releaseExe"
  Write-Host "Build it first: cmake --build cpp/build-vs2017-xp32 --config Release"
  exit 1
}

if ([string]::IsNullOrWhiteSpace($IsccPath)) {
  $candidates = @(
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
    'C:\Program Files (x86)\Inno Setup 5\ISCC.exe',
    'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
    'C:\Program Files\Inno Setup 5\ISCC.exe',
    'C:\Program Files\Inno Setup 6\ISCC.exe'
  )
  foreach ($p in $candidates) {
    if (Test-Path $p) {
      $IsccPath = $p
      break
    }
  }
}

if ([string]::IsNullOrWhiteSpace($IsccPath) -or -not (Test-Path $IsccPath)) {
  Write-Error "ISCC.exe not found. Install Inno Setup (recommended 5.6.1 for best XP compatibility), then run:`n  powershell -ExecutionPolicy Bypass -File cpp\\installer\\build_installer.ps1 -IsccPath 'C:\\Program Files (x86)\\Inno Setup 5\\ISCC.exe'"
  exit 1
}

Write-Host "Using ISCC: $IsccPath"
& $IsccPath $iss
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

Write-Host "Installer built successfully:"
Write-Host (Join-Path $scriptDir 'output')
