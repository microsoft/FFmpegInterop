@setlocal
@echo off

if "%1" == "/?" goto Usage
if "%~1" == "" goto Usage

:: Initialize build configuration
set BUILD.ARM=N
set BUILD.x86=N
set BUILD.x64=N

:: Export full current PATH from environment into MSYS2
set MSYS2_PATH_TYPE=inherit

:: Iterate through arguments and set the right configuration
for %%a in (%*) do (
    if /I "%%a"=="ARM" (
        set BUILD.ARM=Y
    ) else if /I "%%a"=="x86" (
        set BUILD.x86=Y
    ) else if /I "%%a"=="x64" (
        set BUILD.x64=Y
    ) else (
        goto Usage
    )
)

:: Set build all architecture if none are specified
if %BUILD.ARM%==N (
    if %BUILD.x86%==N (
        if %BUILD.x64%==N (
            set BUILD.ARM=Y
            set BUILD.x86=Y
            set BUILD.x64=Y
        )
    )
)

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
if not defined VS140COMNTOOLS (
    echo:
    echo VS140COMNTOOLS environment variable is not found. Check your Visual Studio 2015 installation
    goto Cleanup
)

if defined MSYS2_BIN (
    if exist %MSYS2_BIN% goto Buildx86
)

echo:
echo MSYS2 is needed. Set it up properly and provide the executable path in MSYS2_BIN environment variable. E.g.
echo:
echo     set MSYS2_BIN="C:\msys64\usr\bin\bash.exe"
echo:
echo See https://trac.ffmpeg.org/wiki/CompilationGuide/WinRT#PrerequisitesandFirstTimeSetupInstructions
goto Cleanup


:: Build and deploy library
:Buildx86
if %BUILD.x86%==N goto Buildx64
echo Building FFmpeg for x86...
echo:

setlocal
call "%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" x86 store
set LIB=%VSINSTALLDIR%VC\lib\store;%VSINSTALLDIR%VC\atlmfc\lib;%UniversalCRTSdkDir%lib\%UCRTVersion%\ucrt\x86;;%UniversalCRTSdkDir%lib\%UCRTVersion%\um\x86;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\lib\um\x86;;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\Lib\um\x86
set LIBPATH=%VSINSTALLDIR%VC\atlmfc\lib;%VSINSTALLDIR%VC\lib;
set INCLUDE=%VSINSTALLDIR%VC\include;%VSINSTALLDIR%VC\atlmfc\include;%UniversalCRTSdkDir%Include\%UCRTVersion%\ucrt;%UniversalCRTSdkDir%Include\%UCRTVersion%\um;%UniversalCRTSdkDir%Include\%UCRTVersion%\shared;%UniversalCRTSdkDir%Include\%UCRTVersion%\winrt;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\Include\um;

%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh x86
endlocal

:Buildx64
if %BUILD.x64%==N goto BuildARM
echo Building FFmpeg for x64...
echo:

setlocal
call "%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" x64 store
set LIB=%VSINSTALLDIR%VC\lib\store\amd64;%VSINSTALLDIR%VC\atlmfc\lib\amd64;%UniversalCRTSdkDir%lib\%UCRTVersion%\ucrt\x64;;%UniversalCRTSdkDir%lib\%UCRTVersion%\um\x64;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\lib\um\x64;;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\Lib\um\x64
set LIBPATH=%VSINSTALLDIR%VC\atlmfc\lib\amd64;%VSINSTALLDIR%VC\lib\amd64;
set INCLUDE=%VSINSTALLDIR%VC\include;%VSINSTALLDIR%VC\atlmfc\include;%UniversalCRTSdkDir%Include\%UCRTVersion%\ucrt;%UniversalCRTSdkDir%Include\%UCRTVersion%\um;%UniversalCRTSdkDir%Include\%UCRTVersion%\shared;%UniversalCRTSdkDir%Include\%UCRTVersion%\winrt;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\Include\um;

%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh x64
endlocal

:BuildARM
if %BUILD.ARM%==N goto Cleanup
echo Building FFmpeg for ARM...
echo:

setlocal
call "%VS140COMNTOOLS%..\..\VC\vcvarsall.bat" x86_arm store
set LIB=%VSINSTALLDIR%VC\lib\store\ARM;%VSINSTALLDIR%VC\atlmfc\lib\ARM;%UniversalCRTSdkDir%lib\%UCRTVersion%\ucrt\arm;;%UniversalCRTSdkDir%lib\%UCRTVersion%\um\arm;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\lib\um\arm;;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\Lib\um\arm
set LIBPATH=%VSINSTALLDIR%VC\atlmfc\lib\ARM;%VSINSTALLDIR%VC\lib\ARM;
set INCLUDE=%VSINSTALLDIR%VC\include;%VSINSTALLDIR%VC\atlmfc\include;%UniversalCRTSdkDir%Include\%UCRTVersion%\ucrt;%UniversalCRTSdkDir%Include\%UCRTVersion%\um;%UniversalCRTSdkDir%Include\%UCRTVersion%\shared;%UniversalCRTSdkDir%Include\%UCRTVersion%\winrt;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\Include\um;

%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh ARM
endlocal

goto Cleanup

:: Display help message
:Usage
echo The correct usage is:
echo:
echo     %0 [architecture...]
echo:
echo where
echo:
echo [architecture]    is: x86 ^| x64 ^| ARM (optional)
echo:
echo For example:
echo     %0            - Build all architectures
echo     %0 x64        - Build x64 only
echo     %0 x86 x64    - Build x64 and x86 only
goto :eof

:Cleanup
@endlocal