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

.PARAMETER Patches
Specifies one or more patches or directories containing patches to apply to FFmpeg before building.

.PARAMETER CompilerRsp
Specifies one or more compiler response files (.rsp) to pass to the FFmpeg build.

.PARAMETER Prefast
Specifies one or more rulesets to use for PREfast static analysis.

.PARAMETER SarifLogs
Specifies whether to enable SARIF output diagnostics for MSVC.
https://learn.microsoft.com/en-us/cpp/build/reference/sarif-output

.PARAMETER Fuzzing
Specifies whether to build FFmpeg with fuzzing support.

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
    [string]$Settings,
    [string]$Patches,
    [string[]]$CompilerRsp,
    [string[]]$Prefast,
    [switch]$SarifLogs,
    [switch]$Fuzzing
)

function ApplyFFmpegPatch([string]$path)
{
    if (-not (Test-Path $path))
    {
        Write-Error "ERROR: Patch `"$path`" does not exist."
        exit 1
    }

    if (Test-Path $path -PathType Container)
    {
        # Apply all patches in the directory
        $patches = Get-ChildItem -Path $path
        foreach ($patch in $patches)
        {
            ApplyFFmpegPatch($patch)
        }

        return
    }

    $patch = $path
    Push-Location -Path "$PSScriptRoot\ffmpeg"

    try
    {
        # Check if the patch can be applied cleanly
        & git apply --check $patch -C0 --quiet
        if ($? -eq $true)
        {
            Write-Host "Applying patch: $patch"
            & git apply $patch
            return
        }

        # Check if the patch has already been applied
        & git apply --reverse --check $patch -C0 --quiet
        if ($? -eq $true)
        {
            Write-Host "Patch `"$patch`" has already been applied."
        }
        else
        {
            Write-Error "ERROR: Patch `"$patch`" could not be applied."
            exit 1
        }
    }
    finally
    {
        Pop-Location
    }
}

# Validate FFmpeg submodule
$configure = "$PSScriptRoot\ffmpeg\configure"
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

ApplyFFmpegPatch("$PSScriptRoot\patches")

if ($Patches)
{
    foreach ($patch in $Patches)
    {
        ApplyFFmpegPatch($patch)
    }
}

if ($CompilerRsp)
{
    $CompilerRsp = $CompilerRsp | ForEach-Object {
        if (-not (Test-Path $_))
        {
            Write-Error "ERROR: Compiler response file `"$_`" does not exist."
            exit 1
        }

        Resolve-Path $_
    }
}

if ($Prefast)
{
    $Prefast = $Prefast | ForEach-Object {
        if (-not (Test-Path $_))
        {
            Write-Error "ERROR: PREfast ruleset `"$_`" does not exist."
            exit 1
        }

        Resolve-Path $_
    }
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

# Build FFmpeg for each specified architecture
foreach ($arch in $Architectures)
{
    Write-Host "Building FFmpeg for $arch..."

    # Initialize the build environment
    Enter-VsDevShell `
        -VsInstallPath $vsInstance.InstallationPath `
        -DevCmdArguments "-arch=$arch -host_arch=$hostArch -app_platform=$AppPlatform -vcvars_spectre_libs=spectre" `
        -SkipAutomaticLocation `
        -DevCmdDebugLevel $debugLevel

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

    if ($CompilerRsp)
    {
        $opts += '--compiler-rsp', "$($CompilerRsp -join ';')"
    }

    if ($Prefast)
    {
        $opts += '--prefast', "$($Prefast -join ';')"
    }

    if ($SarifLogs)
    {
        $opts += '--sarif-logs'
    }

    # Fuzzing requires libraries in the $VCToolsInstallDir to be linked
    if ($Fuzzing)
    {
        $fuzzingLibPath = Join-Path -Path $env:VCToolsInstallDir -ChildPath "lib\$arch"
        if (-not (Test-Path $fuzzingLibPath))
        {
            Write-Error "ERROR: $fuzzingLibPath does not exist. Ensure the Visual Studio installation is correct."
            exit 1
        }

        Write-Host "Adding $fuzzingLibPath to the LIB environment variable"
        $env:LIB = "$env:LIB;$fuzzingLibPath"

        $opts += '--fuzzing'
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
