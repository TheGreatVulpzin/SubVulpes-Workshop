# Builds one plugin folder with the SDK, and copies the DLL that plugin.ini names into that folder.
# Usage: build_plugin.ps1 -Folder official\plugins\sv.permissions [-All]
# Set SUBVULPES_SDK to the SDK folder. Load the 32-bit build tools first (. tools/vcvars.ps1).
# -All also builds the test targets of the folder. The folder keeps its build in build\ID at the root of the repository.
param([Parameter(Mandatory = $true)][string]$Folder, [switch]$All)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not $env:SUBVULPES_SDK -or -not (Test-Path (Join-Path $env:SUBVULPES_SDK 'tools\sv_plugin.cmake'))) { throw 'Set SUBVULPES_SDK to the SDK folder.' }
$folder = (Resolve-Path -LiteralPath $Folder).Path
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$manifest = Get-Content -LiteralPath (Join-Path $folder 'plugin.ini')
$id = ($manifest | Where-Object { $_ -match '^id=' } | Select-Object -First 1) -replace '^id=', ''
$library = ($manifest | Where-Object { $_ -match '^library=' } | Select-Object -First 1) -replace '^library=', ''
if (-not $id -or -not $library) { throw "plugin.ini in $folder needs id= and library=." }

$out = Join-Path $repo "build\$id"
cmake -S $folder -B $out -G 'NMake Makefiles' -DCMAKE_BUILD_TYPE=Release "-DSV_SDK_ROOT=$env:SUBVULPES_SDK"
if ($LASTEXITCODE -ne 0) { throw 'cmake failed.' }
if ($All) { cmake --build $out } else { cmake --build $out --target ($id -replace '\.', '-') }
if ($LASTEXITCODE -ne 0) { throw 'The build failed.' }
Copy-Item -LiteralPath (Join-Path $out "plugins\$id\$library") -Destination (Join-Path $folder $library) -Force
Write-Output "OK: $library"
