#!/usr/bin/env powershell
###################################################
# Copyright (C) Microsoft. All rights reserved.
###################################################

<#
.SYNOPSIS
Sets up the FFmpeg build environment.

.DESCRIPTION
Sets up the FFmpeg build environment.

.INPUTS
None. You cannot pipe objects to SetUpFFmpegBuildEnvironment.ps1.

.OUTPUTS
None. SetUpFFmpegBuildEnvironment.ps1 does not generate any output.

.EXAMPLE
SetUpFFmpegBuildEnvironment.ps1

.LINK
https://trac.ffmpeg.org/wiki/CompilationGuide/WinRT
#>

$install_dir = 'C:'
$msys2_root = "$install_dir\msys64"
$msys2_bin = "$msys2_root\usr\bin"

# Get the latest MSYS2 release
Write-Host 'Fetching releases...'
$repo = 'msys2/msys2-installer'
$url = "https://api.github.com/repos/$repo/releases"
$releases = Invoke-WebRequest $url -UseBasicParsing | ConvertFrom-Json
$release = $releases[0]
Write-Host "Latest release: $($release.tag_name)"

# Download msys2-base-x86_64-YYYYMMDD.tar.xz
$archive = "msys2-base-x86_64-$($release.tag_name.replace('-', '')).tar.xz"
Write-Host "Downloading $archive..."
$archive_url = ($release.assets | where name -eq $archive).browser_download_url
Invoke-WebRequest $archive_url -OutFile $archive -UseBasicParsing

# Delete install directory if it already exists
if (Test-Path $msys2_root)
{
    Write-Host "Deleting $msys2_root..."
    Remove-Item -Recurse -Force $msys2_root
}

# Extract msys2-base-x86_64-YYYYMMDD.tar.xz
if (Get-Command 7z -ErrorAction Ignore)
{
    Write-Host "Extracting $archive to $msys2_root with 7z..."
    cmd /c "7z x $archive -so | 7z x -si -ttar -o$install_dir -bso0"
}
elseif (Get-Command tar -ErrorAction Ignore)
{
    Write-Host "Extracting $archive to $msys2_root with tar..."
    tar -xvf $archive -C $install_dir
}
else
{
    Write-Error 'Please install 7z.exe or tar.exe and add to $env:PATH.'
    exit 1
}

Remove-Item $archive

# Set MSYS2_PATH_TYPE=inherit in msys2_shell.cmd
$msys2_shell = "$msys2_root\msys2_shell.cmd"
Write-Host "Modifying $msys2_shell..."
(Get-Content $msys2_shell).replace('rem set MSYS2_PATH_TYPE=inherit', 'set MSYS2_PATH_TYPE=inherit') | Set-Content $msys2_shell

# Update packages
Write-Host 'Updating packages...'
for ($i = 0; $i -lt 2; $i++)
{
    Start-Process -Wait $msys2_shell -ArgumentList '-c "pacman -Syuu --noconfirm"'
}

# Install additional packages 
$packages = @('make', 'gcc', 'diffutils', 'yasm')
foreach ($package in $packages)
{
    Write-Host "Installing $package..."
    Start-Process -Wait $msys2_shell -ArgumentList "-c `"pacman -S $package --noconfirm`""
}

# Rename link.exe to prevent conflict with MSVC
$link = "$msys2_bin\link.exe"
Write-Host "Renaming $link..."
Rename-Item -Path $link -NewName "$link.0000"

# Download gas-preprocessor.pl
Write-Host "Downloading gas-preprocessor.pl to $msys2_bin\gas-preprocessor.pl..."
$url = 'https://raw.githubusercontent.com/FFmpeg/gas-preprocessor/master/gas-preprocessor.pl'
Invoke-WebRequest $url -OutFile "$msys2_bin\gas-preprocessor.pl" -UseBasicParsing

# Set MSYS2_BIN environment variable
Write-Host 'Setting MSYS2_BIN environment variable...'
[System.Environment]::SetEnvironmentVariable('MSYS2_BIN', "$msys2_bin\bash.exe", "User")
