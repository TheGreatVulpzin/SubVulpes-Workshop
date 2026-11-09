# Builds every plugin under official\plugins, then runs the tests of each one with the runner of the SDK.
# Usage: run_plugin_tests.ps1 [-Only sv.permissions,sv.admcmd]
# Set SUBVULPES_SDK to the SDK folder. Load the 32-bit build tools first (. tools/vcvars.ps1).
#
# The tests expect a folder with the runner and one folder for each plugin: build\stage\subvulpes_plugin_test.exe and
# build\stage\plugins\ID\. This script makes that folder from the SDK and from the build of each plugin.
# A plugin folder may hold tests\*_test.py (called with the stage folder) and CMake test programs named *-test.exe.
param([string[]]$Only = @())
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not $env:SUBVULPES_SDK -or -not (Test-Path (Join-Path $env:SUBVULPES_SDK 'tools\subvulpes_plugin_test.exe'))) {
    throw 'Set SUBVULPES_SDK to the SDK folder. It must hold tools\subvulpes_plugin_test.exe.'
}
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$stage = Join-Path $repo 'build\stage'
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path (Join-Path $stage 'plugins'), (Join-Path $stage 'tests') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $env:SUBVULPES_SDK 'tools\subvulpes_plugin_test.exe') -Destination $stage
Copy-Item -LiteralPath (Join-Path $env:SUBVULPES_SDK 'tools\probe') -Destination (Join-Path $stage 'plugins\sv.probe') -Recurse

$folders = @(Get-ChildItem (Join-Path $repo 'official\plugins') -Directory | Where-Object { (Test-Path (Join-Path $_.FullName 'plugin.ini')) -and (Test-Path (Join-Path $_.FullName 'CMakeLists.txt')) })
$selected = if ($Only.Count) { @($folders | Where-Object { $Only -contains $_.Name }) } else { $folders }
if (-not $selected.Count) { throw 'No plugin folder selected.' }

# Build all of them. A test may use several plugins together. -Only selects which ones are tested.
foreach ($folder in $folders) {
    Write-Output "== Build $($folder.Name)"
    & (Join-Path $PSScriptRoot 'build_plugin.ps1') -Folder $folder.FullName -All
    if ($LASTEXITCODE) { throw "The build of $($folder.Name) failed." }
    Get-ChildItem (Join-Path $repo "build\$($folder.Name)\plugins") -Directory | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $stage 'plugins') -Recurse -Force
    }
}

$failed = @()
foreach ($folder in $selected) {
    Get-ChildItem (Join-Path $repo "build\$($folder.Name)") -Filter '*-test.exe' -File | ForEach-Object {
        Write-Output "== $($folder.Name): $($_.Name)"
        & $_.FullName
        if ($LASTEXITCODE) { $failed += "$($folder.Name)/$($_.Name)" }
    }
    $tests = Join-Path $folder.FullName 'tests'
    if (Test-Path $tests) {
        Get-ChildItem $tests -Filter '*_test.py' -File | ForEach-Object {
            Write-Output "== $($folder.Name): $($_.Name)"
            python -I $_.FullName $stage
            if ($LASTEXITCODE) { $failed += "$($folder.Name)/$($_.Name)" }
        }
    }
}
if ($failed.Count) { throw ('Failed: ' + ($failed -join ', ')) }
Write-Output 'OK: all plugin tests passed.'
