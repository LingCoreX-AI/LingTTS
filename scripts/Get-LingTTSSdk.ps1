param([string]$Destination = "$PSScriptRoot\..\.downloads")
$ErrorActionPreference = 'Stop'

$version = '0.2.0-rc1'
$asset = "lingtts-sdk-$version-community.tar.gz"
$base = "https://github.com/LingCoreX-AI/LingTTS/releases/download/v$version"
$download = Join-Path ([System.IO.Path]::GetTempPath()) ([guid]::NewGuid().ToString())
$archive = Join-Path $download $asset
$checksum = "$archive.sha256"
$null = New-Item -ItemType Directory -Force -Path $download
$null = New-Item -ItemType Directory -Force -Path $Destination
try {
    Invoke-WebRequest -Uri "$base/$asset" -OutFile $archive
    Invoke-WebRequest -Uri "$base/$asset.sha256" -OutFile $checksum
    $expected = ((Get-Content $checksum -Raw).Trim() -split '\s+')[0].ToLowerInvariant()
    $actual = (Get-FileHash -Algorithm SHA256 $archive).Hash.ToLowerInvariant()
    if ($actual -ne $expected) { throw "SHA-256 mismatch for $asset" }
    tar.exe -xzf $archive -C $Destination
    if ($LASTEXITCODE -ne 0) { throw 'tar.exe failed to extract the SDK archive' }
}
finally {
    Remove-Item -Recurse -Force $download -ErrorAction SilentlyContinue
}
$sdk = Join-Path $Destination "lingtts-sdk-$version-community"
Write-Host "SDK ready: $sdk"
Write-Host "From that directory, run: idf.py -C examples/hello -B build/hello build"
