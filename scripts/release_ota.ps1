param(
  [string]$ManifestDir = "C:\Users\mikelch\Documents\PlatformIO\Projects\tmp",
  [string]$ArchiveDir = "C:\Users\mikelch\Documents\PlatformIO\Projects\FirmwareArchive\ManCaveScroller",
  [string]$Env = "esp32doit-devkit-v1",
  [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-NextPatchVersion {
  param([string]$VersionText)
  $match = [regex]::Match($VersionText, '(\d+)\.(\d+)\.(\d+)')
  if (-not $match.Success) {
    throw "Manifest version '$VersionText' is not in MAJOR.MINOR.PATCH format."
  }
  $major = [int]$match.Groups[1].Value
  $minor = [int]$match.Groups[2].Value
  $patch = [int]$match.Groups[3].Value + 1
  return "$major.$minor.$patch"
}

function Set-FirmwareVersionBuildFlag {
  param(
    [string]$IniPath,
    [string]$Version
  )

  $iniRaw = Get-Content $IniPath -Raw
  $updated = [regex]::Replace(
    $iniRaw,
    '-DAPP_FIRMWARE_VERSION=\\\"[^\\\"]+\\\"',
    ('-DAPP_FIRMWARE_VERSION=\"{0}\"' -f $Version)
  )

  if ($updated -eq $iniRaw) {
    throw "Could not find APP_FIRMWARE_VERSION build flag in $IniPath"
  }

  Set-Content -Path $IniPath -Value $updated -NoNewline
}

$projectDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$manifestPath = Join-Path $ManifestDir "manifest.json"
$platformioIni = Join-Path $projectDir "platformio.ini"

if (-not (Test-Path $manifestPath)) {
  throw "Manifest not found: $manifestPath"
}
if (-not (Test-Path $platformioIni)) {
  throw "platformio.ini not found: $platformioIni"
}

$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
if (-not $manifest.version) {
  throw "Manifest missing 'version'"
}
if (-not $manifest.firmware) {
  throw "Manifest missing 'firmware' object"
}

$nextVersion = Get-NextPatchVersion -VersionText ([string]$manifest.version)
$newBinName = "${nextVersion}_ManCave.bin"
$destBinPath = Join-Path $ManifestDir $newBinName
$archiveBinPath = Join-Path $ArchiveDir $newBinName

if ($DryRun) {
  Write-Host "[DryRun] Next version: $nextVersion"
  Write-Host "[DryRun] New binary name: $newBinName"
  Write-Host "[DryRun] Archive binary path: $archiveBinPath"
} else {
  Set-FirmwareVersionBuildFlag -IniPath $platformioIni -Version $nextVersion
}

Push-Location $projectDir
try {
  if ($DryRun) {
    Write-Host "[DryRun] Would run: pio run -e $Env"
  } else {
    & pio run -e $Env
  }
} finally {
  Pop-Location
}

$builtBinPath = Join-Path $projectDir ".pio\build\$Env\firmware.bin"
if (-not (Test-Path $builtBinPath)) {
  throw "Compiled firmware not found: $builtBinPath"
}

if ($DryRun) {
  Write-Host "[DryRun] Would copy $builtBinPath -> $destBinPath"
  Write-Host "[DryRun] Would copy $builtBinPath -> $archiveBinPath"
  exit 0
}

if (-not (Test-Path $ArchiveDir)) {
  New-Item -Path $ArchiveDir -ItemType Directory -Force | Out-Null
}

Copy-Item -Path $builtBinPath -Destination $destBinPath -Force
Copy-Item -Path $builtBinPath -Destination $archiveBinPath -Force

$size = (Get-Item $destBinPath).Length
$md5 = (Get-FileHash $destBinPath -Algorithm MD5).Hash.ToLower()
$sha256 = (Get-FileHash $destBinPath -Algorithm SHA256).Hash.ToLower()

$manifest.version = $nextVersion
$manifest.build_date = (Get-Date).ToString("yyyy-MM-dd")

$firmwareUrl = [string]$manifest.firmware.url
if ([string]::IsNullOrWhiteSpace($firmwareUrl)) {
  $manifest.firmware.url = $newBinName
} else {
  $lastSlash = $firmwareUrl.LastIndexOf('/')
  if ($lastSlash -ge 0) {
    $prefix = $firmwareUrl.Substring(0, $lastSlash + 1)
    $manifest.firmware.url = "$prefix$newBinName"
  } else {
    $manifest.firmware.url = $newBinName
  }
}

$manifest.firmware.size = [int64]$size
$manifest.firmware.md5 = $md5
$manifest.firmware.sha256 = $sha256

$manifestJson = $manifest | ConvertTo-Json -Depth 16
Set-Content -Path $manifestPath -Value $manifestJson

Write-Host "Release artifact created:"
Write-Host "  Version:   $nextVersion"
Write-Host "  Binary:    $destBinPath"
Write-Host "  Archive:   $archiveBinPath"
Write-Host "  Size:      $size"
Write-Host "  MD5:       $md5"
Write-Host "  SHA256:    $sha256"
Write-Host "  Manifest:  $manifestPath"
