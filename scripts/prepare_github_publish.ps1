param(
  [string]$SourceRoot = (Split-Path -Parent $PSScriptRoot),
  [string]$PublishRoot = ""
)

$SourceRoot = [System.IO.Path]::GetFullPath($SourceRoot)
if ([string]::IsNullOrWhiteSpace($PublishRoot)) {
  $PublishRoot = "${SourceRoot}_github_publish"
}
$PublishRoot = [System.IO.Path]::GetFullPath($PublishRoot)

if ($PublishRoot -ieq $SourceRoot -or $PublishRoot.StartsWith($SourceRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
  throw "PublishRoot must not be inside SourceRoot. Choose a folder outside the working tree."
}

$secret = ""

function Should-ExcludePath([string]$FullPath) {
  $leaf = Split-Path $FullPath -Leaf
  $skipNames = @(
    ".git", ".vs", "__pycache__", "build", "build-vs", "build-vs2017-xp32",
    "dist", "tmp_fluent", "_github_publish"
  )
  foreach ($name in $skipNames) {
    if ($leaf -ieq $name) { return $true }
    if ($FullPath -match "(^|[\\/])$([regex]::Escape($name))([\\/]|$)") { return $true }
  }
  return $false
}

if (Test-Path $PublishRoot) {
  Remove-Item -Recurse -Force $PublishRoot
}
New-Item -ItemType Directory -Force -Path $PublishRoot | Out-Null

Write-Host "Copying source tree to $PublishRoot ..."
Get-ChildItem -LiteralPath $SourceRoot -Force -Recurse | ForEach-Object {
  $full = $_.FullName
  if ($full -eq $PublishRoot) { return }
  if (Should-ExcludePath $full) { return }

  $relative = $full.Substring($SourceRoot.Length).TrimStart('\','/')
  $dest = Join-Path $PublishRoot $relative

  if ($_.PSIsContainer) {
    New-Item -ItemType Directory -Force -Path $dest | Out-Null
  } else {
    $parent = Split-Path $dest -Parent
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    Copy-Item -LiteralPath $full -Destination $dest -Force
  }
}

Write-Host "Redacting hardcoded secret strings in publish copy..."
$textExts = @(".py", ".ps1", ".nsi", ".iss", ".txt", ".md", ".cmake", ".c", ".cpp", ".h", ".rc", ".json", ".ini", ".toml")
Get-ChildItem -LiteralPath $PublishRoot -Recurse -File | ForEach-Object {
  if ($textExts -contains $_.Extension.ToLowerInvariant()) {
    try {
      $content = Get-Content -LiteralPath $_.FullName -Raw -ErrorAction Stop
      if ($content.Contains($secret)) {
        $content = $content.Replace($secret, "")
        Set-Content -LiteralPath $_.FullName -Value $content -NoNewline
      }
    } catch {
      # Skip files that can't be read as text.
    }
  }
}

Write-Host "Publish copy ready: $PublishRoot"
