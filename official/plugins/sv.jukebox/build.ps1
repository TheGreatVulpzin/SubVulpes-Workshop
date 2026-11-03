# Builds sv.jukebox. The source lives in the SubVulpes repository, where its tests run.
# This script downloads the source at one commit and checks each file against source.sha256. Then it builds the DLL with the SDK.
# To update the plugin: change $commit, replace plugin.ini and source.sha256 with the new ones, and open a pull request.
# Set SUBVULPES_SDK to the SDK folder. Load the 32-bit build tools first (tools/vcvars.ps1).
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$commit = 'ca272b457eb7aa0caf54fbdd20b1737915fa4b46'
$base = "https://raw.githubusercontent.com/TheGreatVulpzin/SubVulpes/$commit/plugins/jukebox/"
if (-not $env:SUBVULPES_SDK -or -not (Test-Path (Join-Path $env:SUBVULPES_SDK 'tools\sv_plugin.cmake'))) { throw 'Set SUBVULPES_SDK to the SDK folder.' }

$work = Join-Path $env:TEMP ('sv-jukebox-' + [guid]::NewGuid().ToString('N'))
$source = Join-Path $work 'src'
New-Item -ItemType Directory -Path $source | Out-Null
try {
    foreach ($line in Get-Content (Join-Path $PSScriptRoot 'source.sha256')) {
        if ($line -cnotmatch '^([0-9a-f]{64})  ([A-Za-z0-9._-]+)$') { throw "source.sha256 has a bad line: $line" }
        $hash, $name = $Matches[1], $Matches[2]
        $target = Join-Path $source $name
        Invoke-WebRequest -Uri ($base + $name) -OutFile $target -UseBasicParsing
        $actual = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $hash) { throw "The file $name does not match its pinned hash." }
    }
    # The manifest of the Workshop must equal the manifest of the source.
    $manifest = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'plugin.ini') -Algorithm SHA256).Hash
    if ($manifest -ne (Get-FileHash -LiteralPath (Join-Path $source 'plugin.ini') -Algorithm SHA256).Hash) { throw 'plugin.ini does not match the pinned source.' }

    $out = Join-Path $work 'out'
    cmake -S $source -B $out -G 'NMake Makefiles' -DCMAKE_BUILD_TYPE=Release "-DSV_SDK_ROOT=$env:SUBVULPES_SDK"
    if ($LASTEXITCODE -ne 0) { throw 'cmake failed.' }
    cmake --build $out --target sv-jukebox
    if ($LASTEXITCODE -ne 0) { throw 'The build failed.' }
    Copy-Item -LiteralPath (Join-Path $out 'plugins\sv.jukebox\jukebox.dll') -Destination (Join-Path $PSScriptRoot 'jukebox.dll') -Force
} finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
Write-Output 'OK: jukebox.dll'
