# Builds sv.permissions. The source and the tests are in this folder.
# Set SUBVULPES_SDK to the SDK folder. Load the 32-bit build tools first (. tools/vcvars.ps1).
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot '..\..\..\tools\build_plugin.ps1') -Folder $PSScriptRoot
if ($LASTEXITCODE) { exit $LASTEXITCODE }
