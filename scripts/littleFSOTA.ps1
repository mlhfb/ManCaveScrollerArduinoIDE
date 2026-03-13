param(
  [string]$ManifestDir = "C:\Users\mikelch\Documents\PlatformIO\Projects\tmp",
  [string]$ArchiveDir = "C:\Users\mikelch\Documents\PlatformIO\Projects\FirmwareArchive\ManCaveScroller",
  [string]$Env = "esp32doit-devkit-v1",
  [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-ObjectPropertyValue {
  param(
    [object]$Object,
    [string]$Name,
    [string]$Default = ""
  )

  if ($null -eq $Object) {
    return $Default
  }
  $prop = $Object.PSObject.Properties[$Name]
  if ($null -eq $prop -or $null -eq $prop.Value) {
    return $Default
  }
  return [string]$prop.Value
}

function Set-ObjectPropertyValue {
  param(
    [object]$Object,
    [string]$Name,
    [object]$Value
  )

  if ($null -eq $Object) {
    throw "Cannot set property '$Name' on null object."
  }

  $prop = $Object.PSObject.Properties[$Name]
  if ($null -eq $prop) {
    $Object | Add-Member -NotePropertyName $Name -NotePropertyValue $Value
  } else {
    $Object.$Name = $Value
  }
}

function Get-NextLittleFsVersion {
  param(
    [object]$Manifest,
    [string]$ManifestDirPath
  )

  $candidate = ""
  if ($Manifest.PSObject.Properties.Name -contains "littlefs" -and
      $null -ne $Manifest.littlefs) {
    $candidate = Get-ObjectPropertyValue -Object $Manifest.littlefs -Name "version" -Default ""
  }

  $match = [regex]::Match($candidate, '^v(\d+)$', 'IgnoreCase')
  if ($match.Success) {
    return "v$([int]$match.Groups[1].Value + 1)"
  }

  $maxVersion = 0
  if (Test-Path $ManifestDirPath) {
    $files = Get-ChildItem -Path $ManifestDirPath -Filter "*_littlefs.bin" -File
    foreach ($file in $files) {
      $nameMatch = [regex]::Match($file.Name, '^v(\d+)_littlefs\.bin$', 'IgnoreCase')
      if ($nameMatch.Success) {
        $v = [int]$nameMatch.Groups[1].Value
        if ($v -gt $maxVersion) {
          $maxVersion = $v
        }
      }
    }
  }

  return "v$($maxVersion + 1)"
}

function Resolve-UpdatedUrl {
  param(
    [string]$CurrentUrl,
    [string]$NewFileName,
    [string]$FallbackPrefix
  )

  if (-not [string]::IsNullOrWhiteSpace($CurrentUrl)) {
    $lastSlash = $CurrentUrl.LastIndexOf('/')
    if ($lastSlash -ge 0) {
      $prefix = $CurrentUrl.Substring(0, $lastSlash + 1)
      return "$prefix$NewFileName"
    }
  }

  if (-not [string]::IsNullOrWhiteSpace($FallbackPrefix)) {
    $cleanPrefix = $FallbackPrefix
    if (-not $cleanPrefix.EndsWith('/')) {
      $cleanPrefix += '/'
    }
    return "$cleanPrefix$NewFileName"
  }

  return $NewFileName
}

$projectDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$manifestPath = Join-Path $ManifestDir "manifest.json"

if (-not (Test-Path $manifestPath)) {
  throw "Manifest not found: $manifestPath"
}

$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$nextLittleFsVersion = Get-NextLittleFsVersion -Manifest $manifest -ManifestDirPath $ManifestDir
$newBinName = "${nextLittleFsVersion}_littlefs.bin"
$destBinPath = Join-Path $ManifestDir $newBinName
$archiveBinPath = Join-Path $ArchiveDir $newBinName

if ($DryRun) {
  Write-Host "[DryRun] Next littlefs version: $nextLittleFsVersion"
  Write-Host "[DryRun] New binary name: $newBinName"
  Write-Host "[DryRun] Archive binary path: $archiveBinPath"
} else {
  if (-not (Test-Path $ArchiveDir)) {
    New-Item -Path $ArchiveDir -ItemType Directory -Force | Out-Null
  }
}

Push-Location $projectDir
try {
  if ($DryRun) {
    Write-Host "[DryRun] Would run: pio run -e $Env -t buildfs"
  } else {
    & pio run -e $Env -t buildfs
  }
} finally {
  Pop-Location
}

$builtBinPath = Join-Path $projectDir ".pio\build\$Env\littlefs.bin"
if (-not (Test-Path $builtBinPath)) {
  throw "Compiled littlefs image not found: $builtBinPath"
}

if ($DryRun) {
  Write-Host "[DryRun] Would copy $builtBinPath -> $destBinPath"
  Write-Host "[DryRun] Would copy $builtBinPath -> $archiveBinPath"
  exit 0
}

Copy-Item -Path $builtBinPath -Destination $destBinPath -Force
Copy-Item -Path $builtBinPath -Destination $archiveBinPath -Force

$size = (Get-Item $destBinPath).Length
$md5 = (Get-FileHash $destBinPath -Algorithm MD5).Hash.ToLower()
$sha256 = (Get-FileHash $destBinPath -Algorithm SHA256).Hash.ToLower()

if (-not ($manifest.PSObject.Properties.Name -contains "littlefs") -or
    $null -eq $manifest.littlefs) {
  $manifest | Add-Member -NotePropertyName littlefs -NotePropertyValue ([pscustomobject]@{})
}

$fallbackPrefix = ""
if ($manifest.PSObject.Properties.Name -contains "firmware" -and
    $null -ne $manifest.firmware) {
  $firmwareUrl = Get-ObjectPropertyValue -Object $manifest.firmware -Name "url" -Default ""
  $lastSlash = $firmwareUrl.LastIndexOf('/')
  if ($lastSlash -ge 0) {
    $fallbackPrefix = $firmwareUrl.Substring(0, $lastSlash + 1)
  }
}

$currentLittleFsUrl = Get-ObjectPropertyValue -Object $manifest.littlefs -Name "url" -Default ""
Set-ObjectPropertyValue -Object $manifest.littlefs -Name "version" -Value $nextLittleFsVersion
Set-ObjectPropertyValue -Object $manifest.littlefs -Name "url" -Value (Resolve-UpdatedUrl -CurrentUrl $currentLittleFsUrl `
  -NewFileName $newBinName -FallbackPrefix $fallbackPrefix)
Set-ObjectPropertyValue -Object $manifest.littlefs -Name "size" -Value ([int64]$size)
Set-ObjectPropertyValue -Object $manifest.littlefs -Name "md5" -Value $md5
Set-ObjectPropertyValue -Object $manifest.littlefs -Name "sha256" -Value $sha256
Set-ObjectPropertyValue -Object $manifest -Name "build_date" -Value ((Get-Date).ToString("yyyy-MM-dd"))

$manifestJson = $manifest | ConvertTo-Json -Depth 16
Set-Content -Path $manifestPath -Value $manifestJson

Write-Host "LittleFS artifact created:"
Write-Host "  Version:   $nextLittleFsVersion"
Write-Host "  Binary:    $destBinPath"
Write-Host "  Archive:   $archiveBinPath"
Write-Host "  Size:      $size"
Write-Host "  MD5:       $md5"
Write-Host "  SHA256:    $sha256"
Write-Host "  Manifest:  $manifestPath"
