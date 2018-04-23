@setlocal
@echo off

:: Checking that we are running from a clean non-dev cmd

if defined VSINSTALLDIR (
    echo:
    echo This script must be run from a clean cmd environment. 
	echo Do NOT run from VisualStudio Command Prompt such as "ARM Cross Tools Command Prompt".
	echo:
	echo Found variable: VSINSTALLDIR
    goto Cleanup
)

if defined INCLUDE (
    echo:
    echo This script must be run from a clean cmd environment. 
	echo Do NOT run from VisualStudio Command Prompt such as "ARM Cross Tools Command Prompt".
	echo:
	echo Found variable: INCLUDE
    goto Cleanup
)

if defined LIB (
    echo:
    echo This script must be run from a clean cmd environment. 
	echo Do NOT run from VisualStudio Command Prompt such as "ARM Cross Tools Command Prompt".
	echo:
	echo Found variable: LIB
    goto Cleanup
)

if defined LIBPATH (
    echo:
    echo This script must be run from a clean cmd environment. 
	echo Do NOT run from VisualStudio Command Prompt such as "ARM Cross Tools Command Prompt".
	echo:
	echo Found variable: LIBPATH
    goto Cleanup
)

echo:
echo Searching VS Installation folder...

set VSInstallerFolder="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if %PROCESSOR_ARCHITECTURE%==x86 set VSInstallerFolder="%ProgramFiles%\Microsoft Visual Studio\Installer"

pushd %VSInstallerFolder%
for /f "usebackq tokens=*" %%i in (`vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set VSLATESTDIR=%%i
)
popd

echo VS Installation folder: %VSLATESTDIR%

if not exist "%VSLATESTDIR%\VC\Auxiliary\Build\vcvarsall.bat" (
    echo:
    echo VSInstallDir not found or not installed correctly.
    goto Cleanup
)

echo:
echo Checking CPU architecture...

if %PROCESSOR_ARCHITECTURE%==x86 (
    set Comp_x86=x86 uwp 10.0.15063.0
    set Comp_x64=x86_amd64 uwp 10.0.15063.0
    set Comp_ARM=x86_arm uwp 10.0.15063.0
) else (
    set Comp_x86=amd64_x86 uwp 10.0.15063.0
    set Comp_x64=amd64 uwp 10.0.15063.0
    set Comp_ARM=amd64_arm uwp 10.0.15063.0
)

:: Export full current PATH from environment into MSYS2
set MSYS2_PATH_TYPE=inherit

:: Verifying ffmpeg directory
echo Verifying ffmpeg directory...
pushd %~dp0\ffmpeg
if not exist configure (
    echo:
    echo configure is not found in ffmpeg folder. Ensure this folder is populated with ffmpeg snapshot
    goto Cleanup
)
popd

:: Check for required tools

echo:
echo Checking for MSYS2...

if not defined MSYS2_BIN (
    if exist C:\msys64\usr\bin\bash.exe set MSYS2_BIN="C:\msys64\usr\bin\bash.exe"
)

if not defined MSYS2_BIN (
    if exist C:\msys\usr\bin\bash.exe set MSYS2_BIN="C:\msys\usr\bin\bash.exe"
)

if defined MSYS2_BIN (
    if exist %MSYS2_BIN% goto Win10
)

echo:
echo MSYS2 is needed. Set it up properly and provide the executable path in MSYS2_BIN environment variable. E.g.
echo:
echo     set MSYS2_BIN="C:\msys64\usr\bin\bash.exe"
echo:
echo See https://trac.ffmpeg.org/wiki/CompilationGuide/WinRT#PrerequisitesandFirstTimeSetupInstructions
goto Cleanup

:: Build and deploy Windows 10 library
:Win10


:Win10x86
echo Building FFmpeg for Windows 10 apps x86...
echo:

setlocal
call "%VSLATESTDIR%\VC\Auxiliary\Build\vcvarsall.bat" %Comp_x86%

%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh Win10 x86
for /r %~dp0\ffmpeg\Output\Windows10\x86 %%f in (*.pdb) do @copy "%%f" %~dp0\ffmpeg\Build\Windows10\x86\bin
endlocal

:Win10x64
echo Building FFmpeg for Windows 10 apps x64...
echo:

setlocal
call "%VSLATESTDIR%\VC\Auxiliary\Build\vcvarsall.bat" %Comp_x64%

%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh Win10 x64
for /r %~dp0\ffmpeg\Output\Windows10\x64 %%f in (*.pdb) do @copy "%%f" %~dp0\ffmpeg\Build\Windows10\x64\bin
endlocal

:Win10ARM
echo Building FFmpeg for Windows 10 apps ARM...
echo:

setlocal
call "%VSLATESTDIR%\VC\Auxiliary\Build\vcvarsall.bat" %Comp_ARM%

%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh Win10 ARM
for /r %~dp0\ffmpeg\Output\Windows10\ARM %%f in (*.pdb) do @copy "%%f" %~dp0\ffmpeg\Build\Windows10\ARM\bin
endlocal

:Cleanup
@endlocal