# Loads the x86 build tools of Visual Studio into this PowerShell session. Use it with a dot: . tools/vcvars.ps1
# The native plugins are 32-bit, like the server.
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw 'Visual Studio is not installed.' }
$install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $install) { throw 'The C++ build tools are not installed.' }
$batch = Join-Path $install 'VC\Auxiliary\Build\vcvars32.bat'
cmd /c "`"$batch`" >nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2]) }
}
