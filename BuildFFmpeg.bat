@setlocal EnableDelayedExpansion
@echo off

:: Parse the arguments to get the build configuration
set settings_found=0
set build.settings=

for %%a in (%*) do (
    if !settings_found! EQU 1 (
        if not defined build.settings (
            set build.settings=%%a
            set settings_found=0
        ) else (
            echo ERROR: --settings was set more than once 1>&2
            goto Usage
        )
    ) else if /I "%%a"=="x86" (
        set build.archs=!build.archs!;%%a
    ) else if /I "%%a"=="x64" (
        set build.archs=!build.archs!;%%a
    ) else if /I "%%a"=="arm64" (
        set build.archs=!build.archs!;%%a
    ) else if /I "%%a"=="--settings" (
        set settings_found=1
    ) else if /I "%%a"=="--help" (
        goto Usage
    ) else (
        echo ERROR: %%a is an invalid argument 1>&2
        goto Usage
    )
)

:: Verify ffmpeg snapshot
echo Verifying ffmpeg snapshot...

set ffmpeg_configure=%~dp0ffmpeg\configure
if not exist %ffmpeg_configure% (
    echo ERROR: %ffmpeg_configure% does not exist. Ensure an ffmpeg snapshot is populated. 1>&2
    exit /B 1
)

echo %ffmpeg_configure%

:: Verify Visual Studio installation
echo:
echo Verifying Visual Studio installation...

if /I %PROCESSOR_ARCHITECTURE% == x86 (
    set vswhere="%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
) else (
    set vswhere="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
)

if not exist %vswhere% (
    echo ERROR: %vswhere% does not exist. Ensure Visual Studio is installed. 1>&2
    exit /B 1
)

for /f "usebackq tokens=*" %%i in (`%vswhere% -prerelease -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    :: Use the latest version if multiple versions are installed
    set VisualStudioDir=%%i
)

if not defined VisualStudioDir (
    echo ERROR: Could not find a valid Visual Studio installation. Ensure Visual Studio and VC++ x64/x86 build tools are installed. 1>&2
    exit /B 1
)

echo %VisualStudioDir%

:: Verify MSYS2 installation
echo:
echo Verifying MSYS2 installation...

if not defined MSYS2_BIN (
    echo ERROR: MSYS2_BIN environment variable not set. Ensure MSYS2 is installed and set the MSYS2_BIN environment variable to the bash.exe path. 1>&2
    exit /B 1
)

if not exist %MSYS2_BIN% (
    echo ERROR: MSYS2_BIN environment variable is not valid - %MSYS2_BIN% does not exist. Ensure MSYS2 is installed. 1>&2
    exit /B 1
)

echo %MSYS2_BIN%

:: Export full current PATH from environment into MSYS2
set MSYS2_PATH_TYPE=inherit

:: Build ffmpeg for the specified architectures
for %%a in (%build.archs%) do (
    call :build_ffmpeg %%a %build.settings%
    if !ERRORLEVEL! neq 0 (
        echo ERROR: FFmpeg build for %%a failed 1>&2
        exit /B !ERRORLEVEL!
    )
)

exit /B 0


:: ------------------------------------------------------------------------
:build_ffmpeg
setlocal

echo:
echo Building FFmpeg for %1...

:: Determine the correct architecture to pass to vcvarsall.bat
if /I %PROCESSOR_ARCHITECTURE% == x86 (
    if /I %1==x86 (
        set vcvarsall_arch=x86
    ) else if /I %1==x64 (
        set vcvarsall_arch=x86_x64
    ) else if /I %1==arm64 (
        set vcvarsall_arch=x86_arm64
    ) else (
        echo ERROR: %1 is not a valid architecture 1>&2
        exit /B 1
    )
) else (
    if /I %1==x86 (
        set vcvarsall_arch=x64_x86
    ) else if /I %1==x64 (
        set vcvarsall_arch=x64
    ) else if /I %1==arm64 (
        set vcvarsall_arch=x64_arm64
    ) else (
        echo ERROR: %1 is not a valid architecture 1>&2
        exit /B 1
    )
)

:: Call vcvarsall.bat to set up the build environment
call "%VisualStudioDir%\VC\Auxiliary\Build\vcvarsall.bat" %vcvarsall_arch% uwp

:: Build FFmpeg
%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh %*

endlocal
exit /B %ERRORLEVEL%


:: ------------------------------------------------------------------------
:Usage
:: Display help message
echo Syntax:
echo     %0 [arch...] [--settings "<FFmpeg configure settings>"]
echo where:
echo     [arch...]: x86 ^| amd64 ^| arm64
echo:
echo Examples:
echo     %0                                     Build all architectures
echo     %0 x64                                 Build x64
echo     %0 x86 x64                             Build x86 and x64
echo     %0 x64 --settings "--enable-debug"     Build x64 and set --enable-debug for FFmpeg configure

exit /B 0
