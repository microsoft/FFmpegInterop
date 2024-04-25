#!/usr/bin/env powershell
#########################################################
# Copyright (C) Microsoft. All rights reserved.
#########################################################

# TODO: Add help

[CmdletBinding()]
param(
    [Parameter(Mandatory=$true, Position=0)]
    [ValidateSet('x86', 'x64', 'arm', 'arm64')]
    [string[]]$Architectures,

    [ValidateSet('desktop', 'onecore', 'uwp')]
    [string]$AppPlatform = 'desktop',

    [ValidateSet('dynamic', 'hybrid', 'static')]
    [string]$CRT = 'dynamic',

    [string]$Settings = ''
    )

# Validate FFmpeg submodule
$configure = Join-Path $PSScriptRoot 'ffmpeg\configure'
if (-not (Test-Path $configure))
{
    Write-Error "ERROR: $configure does not exist. Ensure the FFmpeg git submodule is initialized and cloned."
    exit 1
}

# Validate FFmpeg build environment
if (-not $env:MSYS2_BIN) {
    Write-Error(
        'ERROR: MSYS2_BIN environment variable not set. ' +
        'Use SetUpFFmpegBuildEnvironment.ps1 to set up the FFmpeg build environment.')
    exit 1
}

if (-not (Test-Path $env:MSYS2_BIN)) {
    Write-Error(
        "ERROR: MSYS2_BIN environment variable is not valid - $env:MSYS2_BIN does not exist. " +
        'Use SetUpFFmpegBuildEnvironment.ps1 to set up the FFmpeg build environment.')
    exit 1
}

# Export full current PATH from environment into MSYS2
$env:MSYS2_PATH_TYPE = 'inherit'

# Locate Visual Studio installation
if (-not (Get-Module -Name VSSetup -ListAvailable))
{
    Install-Module -Name VSSetup -Scope CurrentUser -Force
}

$requiredComponents = 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'
$vsInstance = Get-VSSetupInstance -Prerelease | Select-VSSetupInstance -Require $requiredComponents -Latest
if ($vsInstance -eq $null)
{
    Write-Error(
        'ERROR: Could not find a valid Visual Studio installation. ' +
        'Ensure Visual Studio and VC++ x64/x86 build tools are installed.')
    exit 1
}

# Import Microsoft.VisualStudio.DevShell.dll for Enter-VsDevShell cmdlet
Import-Module "$($vsInstance.InstallationPath)\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"

$hostArch = [System.Environment]::Is64BitOperatingSystem ? 'x64' : 'x86'
$debugLevel = 'None' # Set to Trace for verbose output

# Build FFmpeg for each specified architecture
foreach ($arch in $Architectures)
{
    Write-Host "Building FFmpeg for $arch..."

    # Initialize the build environment
    Enter-VsDevShell `
        -VsInstallPath $vsInstance.InstallationPath `
        -DevCmdArguments "-arch=$arch -host_arch=$hostArch -app_platform=$AppPlatform" `
        -SkipAutomaticLocation `
        -DevCmdDebugLevel $debugLevel

    # Set the options for FFmpegConfig.sh
    $opts = `
        '--arch', $arch, `
        '--app-platform', $AppPlatform, `
        '--crt', $CRT

    if ($Settings)
    {
        $opts += '--settings', $Settings
    }

    # Build FFmpeg
    & $env:MSYS2_BIN --login -x "$PSScriptRoot\FFmpegConfig.sh" $opts
    if (-not $?)
    {
        Write-Error "ERROR: FFmpegConfig.sh failed"
        exit 1
    }
}
