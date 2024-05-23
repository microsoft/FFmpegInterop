#!/usr/bin/env powershell
#########################################################
# Copyright (C) Microsoft. All rights reserved.
#########################################################

<#
.SYNOPSIS
Builds FFmpeg for the specified architecture(s).

.DESCRIPTION
Builds FFmpeg for the specified architecture(s).

.PARAMETER Architectures
Specifies the architecture(s) to build FFmpeg for (x86, x64, arm, and/or arm64).

.PARAMETER AppPlatform
Specifies the target application platform (desktop, onecore, or uwp).

.PARAMETER CRT
Specifies what versions of the C runtime libraries to link against (dynamic, hybrid, or static).

See the following resources for more information about the CRT and STL libraries:
- https://learn.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features
- https://learn.microsoft.com/en-us/cpp/build/reference/md-mt-ld-use-run-time-library

See the following resources for more information about the hybrid CRT:
- https://github.com/microsoft/WindowsAppSDK/blob/main/docs/Coding-Guidelines/HybridCRT.md
- https://www.youtube.com/watch?v=bNHGU6xmUzE&t=977s

.PARAMETER Settings
Specifies options to pass to FFmpeg's configure script.

.INPUTS
None. You cannot pipe objects to BuildFFmpeg.ps1.

.OUTPUTS
None. BuildFFmpeg.ps1 does not generate any output.

.EXAMPLE
BuildFFmpeg.ps1 -Architectures x86,x64,arm64 -AppPlatform uwp -CRT dynamic -Settings "--enable-small"

.LINK
https://learn.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features

.LINK
https://github.com/microsoft/WindowsAppSDK/blob/main/docs/Coding-Guidelines/HybridCRT.md

.LINK
https://www.youtube.com/watch?v=bNHGU6xmUzE&t=977s
#>

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
if (-not $env:MSYS2_BIN)
{
    Write-Error(
        'ERROR: MSYS2_BIN environment variable not set. ' +
        'Use SetUpFFmpegBuildEnvironment.ps1 to set up the FFmpeg build environment.')
    exit 1
}

if (-not (Test-Path $env:MSYS2_BIN))
{
    Write-Error(
        "ERROR: MSYS2_BIN environment variable is not valid - $env:MSYS2_BIN does not exist. " +
        'Use SetUpFFmpegBuildEnvironment.ps1 to set up the FFmpeg build environment.')
    exit 1
}

# Save the original environment state
$originalEnvVars = Get-ChildItem env:

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

# FFmpegConfig.sh sets the -QSpectre option for --extra-cflags to enable Spectre mitigations for the FFmpeg binaries.
# However, --extra-cflags options also get passed to the ARM assembler and -QSpectre causes an A2029 (unknown
# command-line argument) error for ARM/ARM64 builds. To work around this, we modify FFmpeg's configure script to have
# armasm_flags() filter out -Q* options.
$configurePath = "$PSScriptRoot\ffmpeg\configure"
$configure = Get-Content $configurePath -Encoding UTF8 -Raw
if (-not ($configure -match '(?sm)armasm_flags\(\).*?^}'))
{
    Write-Error("ERROR: Failed to find armasm_flags() in $configurePath")
    exit 1
}

$armasm_flags = $matches[0]
if (-not ($armasm_flags.Contains('-Q*)')))
{
    $updated_armasm_flags = $armasm_flags -replace '(?m)(^\s*)\*\)', "`$1-Q*) ;;`n`$&"
    $configure = $configure.Replace($armasm_flags, $updated_armasm_flags )
    $configure | Out-File -Encoding UTF8 $configurePath
}

# TODO: vcvars.bat currently uses the wrong path for OneCore spectre-mitigated libs. Remove this workaround once the issue is fixed.
# https://developercommunity.visualstudio.com/t/vcvarsbat-uses-the-wrong-path-for-OneCo/10664642
$spectreLibs = $AppPlatform -eq 'onecore' ? '' : 'spectre'

# Build FFmpeg for each specified architecture
foreach ($arch in $Architectures)
{
    Write-Host "Building FFmpeg for $arch..."

    # Initialize the build environment
    Enter-VsDevShell `
        -VsInstallPath $vsInstance.InstallationPath `
        -DevCmdArguments "-arch=$arch -host_arch=$hostArch -app_platform=$AppPlatform -vcvars_spectre_libs=$spectreLibs" `
        -SkipAutomaticLocation `
        -DevCmdDebugLevel $debugLevel

    # TODO: vcvars.bat currently uses the wrong path for OneCore spectre-mitigated libs. Remove this workaround once the issue is fixed.
    # https://developercommunity.visualstudio.com/t/vcvarsbat-uses-the-wrong-path-for-OneCo/10664642
    if ($AppPlatform -eq 'onecore')
    {
        $env:LIB = $env:LIB.Replace('onecore', 'spectre\onecore')
        $env:LIBPATH = $env:LIBPATH.Replace('onecore', 'spectre\onecore')
    }

    # Export full current PATH from environment into MSYS2
    $env:MSYS2_PATH_TYPE = 'inherit'

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
    $buildResult = $?

    # Restore the original environment state
    Remove-Item env:*
    foreach ($envVar in $originalEnvVars)
    {
        Set-Item "env:$($envVar.Name)" $envVar.Value
    }

    # Stop if the build failed
    if (-not $buildResult)
    {
        Write-Error "ERROR: FFmpegConfig.sh failed"
        exit 1
    }
}
