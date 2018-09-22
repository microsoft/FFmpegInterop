@setlocal
@echo off

if "%1" == "/?" goto Usage
if "%~1" == "" goto Usage

:: Initialize build configuration
set BUILD.ARM=N
set BUILD.x86=N
set BUILD.x64=N
set BUILD.win7=N
set BUILD.win10=N
set BUILD.win8.1=N
set BUILD.phone8.1=N

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
    ) else if /I "%%a"=="win7" (
        set BUILD.win7=Y
    ) else if /I "%%a"=="win10" (
        set BUILD.win10=Y
    ) else if /I "%%a"=="win8.1" (
        set BUILD.win8.1=Y
    ) else if /I "%%a"=="phone8.1" (
        set BUILD.phone8.1=Y
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

:: Set build all platform if none are specified
if %BUILD.win10%==N (
    if %BUILD.win8.1%==N (
        if %BUILD.phone8.1%==N (
            if %BUILD.win7%==N (
                goto Usage
            )
        )
    )
)

:: Verifying ffmpeg directory
echo Verifying ffmpeg directory...
pushd ffmpeg
if not exist configure (
    echo:
    echo configure is not found in ffmpeg folder. Ensure this folder is populated with ffmpeg snapshot
    goto Cleanup
)
popd

:: Check for required tools
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
if %BUILD.win10%==N goto Win8.1

:: Check for required tools
if not defined VS150COMNTOOLS (
    echo:
    echo VS150COMNTOOLS environment variable is not found. Check your Visual Studio 2017 installation
    goto Cleanup
)

:Win10x86
if %BUILD.x86%==N goto Win10x64
echo Building FFmpeg for Windows 10 apps x86...
echo:

setlocal
call "%VS150COMNTOOLS%\..\..\VC\Auxiliary\Build\vcvarsall.bat" x86 store
set LIB=%VCToolsInstallDir%\lib\x86\store;%VCToolsInstallDir%VC\atlmfc\lib;%UniversalCRTSdkDir%lib\%UCRTVersion%\ucrt\x86;;%UniversalCRTSdkDir%lib\%UCRTVersion%\um\x86;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\lib\um\x86;;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\Lib\um\x86
set LIBPATH=%VCToolsInstallDir%\atlmfc\lib;%VCToolsInstallDir%\lib\x86\;
set INCLUDE=%VCToolsInstallDir%\include;%VCToolsInstallDir%\atlmfc\include;%UniversalCRTSdkDir%Include\%UCRTVersion%\ucrt;%UniversalCRTSdkDir%Include\%UCRTVersion%\um;%UniversalCRTSdkDir%Include\%UCRTVersion%\shared;%UniversalCRTSdkDir%Include\%UCRTVersion%\winrt;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\Include\um;

%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh Win10 x86
endlocal

:Win10x64
if %BUILD.x64%==N goto Win10ARM
echo Building FFmpeg for Windows 10 apps x64...
echo:

setlocal
call "%VS150COMNTOOLS%\..\..\VC\Auxiliary\Build\vcvarsall.bat" x64 store
set LIB=%VCToolsInstallDir%\lib\x64\store;%VCToolsInstallDir%VC\atlmfc\lib\amd64;%UniversalCRTSdkDir%lib\%UCRTVersion%\ucrt\x64;;%UniversalCRTSdkDir%lib\%UCRTVersion%\um\x64;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\lib\um\x64;;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\Lib\um\x64
set LIBPATH=%VCToolsInstallDir%\atlmfc\lib\amd64;%VCToolsInstallDir%\lib\x64\;
set INCLUDE=%VCToolsInstallDir%\include;%VCToolsInstallDir%\atlmfc\include;%UniversalCRTSdkDir%Include\%UCRTVersion%\ucrt;%UniversalCRTSdkDir%Include\%UCRTVersion%\um;%UniversalCRTSdkDir%Include\%UCRTVersion%\shared;%UniversalCRTSdkDir%Include\%UCRTVersion%\winrt;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\Include\um;

%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh Win10 x64
endlocal

:Win10ARM
if %BUILD.ARM%==N goto Win8.1
echo Building FFmpeg for Windows 10 apps ARM...
echo:

setlocal
call "%VS150COMNTOOLS%\..\..\VC\Auxiliary\Build\vcvarsall.bat" x86_arm store
set LIB=%VCToolsInstallDir%\lib\arm\store;%VCToolsInstallDir%VC\atlmfc\lib\ARM;%UniversalCRTSdkDir%lib\%UCRTVersion%\ucrt\arm;;%UniversalCRTSdkDir%lib\%UCRTVersion%\um\arm;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\lib\um\arm;;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\Lib\um\arm
set LIBPATH=%VCToolsInstallDir%\atlmfc\lib\ARM;%VCToolsInstallDir%\lib\arm\;
set INCLUDE=%VCToolsInstallDir%\include;%VCToolsInstallDir%\atlmfc\include;%UniversalCRTSdkDir%Include\%UCRTVersion%\ucrt;%UniversalCRTSdkDir%Include\%UCRTVersion%\um;%UniversalCRTSdkDir%Include\%UCRTVersion%\shared;%UniversalCRTSdkDir%Include\%UCRTVersion%\winrt;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.6\Include\um;

%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh Win10 ARM
endlocal

:: Build and deploy Windows 8.1 library
:Win8.1
if %BUILD.win8.1%==N goto Phone8.1

:: Check for required tools
if not defined VS120COMNTOOLS (
    echo:
    echo VS120COMNTOOLS environment variable is not found. Check your Visual Studio 2013 installation
    goto Cleanup
)

:Win8.1x86
if %BUILD.x86%==N goto Win8.1x64
echo Building FFmpeg for Windows 8.1 apps x86...
echo:

setlocal
call "%VS120COMNTOOLS%..\..\VC\vcvarsall.bat" x86
set LIB=%VSINSTALLDIR%VC\lib\store;%VSINSTALLDIR%VC\atlmfc\lib;%WindowsSdkDir%lib\winv6.3\um\x86;;
set LIBPATH=%WindowsSdkDir%References\CommonConfiguration\Neutral;;%VSINSTALLDIR%VC\atlmfc\lib;%VSINSTALLDIR%VC\lib;
set INCLUDE=%VSINSTALLDIR%VC\include;%VSINSTALLDIR%VC\atlmfc\include;%WindowsSdkDir%Include\um;%WindowsSdkDir%Include\shared;%WindowsSdkDir%Include\winrt;;

%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh Win8.1 x86
endlocal

:Win8.1x64
if %BUILD.x64%==N goto Win8.1ARM
echo Building FFmpeg for Windows 8.1 apps x64...
echo:

setlocal
call "%VS120COMNTOOLS%..\..\VC\vcvarsall.bat" x64
set LIB=%VSINSTALLDIR%VC\lib\store\amd64;%VSINSTALLDIR%VC\atlmfc\lib\amd64;%WindowsSdkDir%lib\winv6.3\um\x64;;
set LIBPATH=%WindowsSdkDir%References\CommonConfiguration\Neutral;;%VSINSTALLDIR%VC\atlmfc\lib\amd64;%VSINSTALLDIR%VC\lib\amd64;
set INCLUDE=%VSINSTALLDIR%VC\include;%VSINSTALLDIR%VC\atlmfc\include;%WindowsSdkDir%Include\um;%WindowsSdkDir%Include\shared;%WindowsSdkDir%Include\winrt;;

%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh Win8.1 x64
endlocal

:Win8.1ARM
if %BUILD.ARM%==N goto Phone8.1
echo Building FFmpeg for Windows 8.1 apps ARM...
echo:

setlocal
call "%VS120COMNTOOLS%..\..\VC\vcvarsall.bat" x86_arm
set LIB=%VSINSTALLDIR%VC\lib\store\ARM;%VSINSTALLDIR%VC\atlmfc\lib\ARM;%WindowsSdkDir%lib\winv6.3\um\arm;;
set LIBPATH=%WindowsSdkDir%References\CommonConfiguration\Neutral;;%VSINSTALLDIR%VC\atlmfc\lib\ARM;%VSINSTALLDIR%VC\lib\ARM;
set INCLUDE=%VSINSTALLDIR%VC\include;%VSINSTALLDIR%VC\atlmfc\include;%WindowsSdkDir%Include\um;%WindowsSdkDir%Include\shared;%WindowsSdkDir%Include\winrt;;

%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh Win8.1 ARM
endlocal

:: Build and deploy Windows Phone 8.1 library
:Phone8.1
if %BUILD.phone8.1%==N goto Win7

:: Check for required tools
if not defined VS120COMNTOOLS (
    echo:
    echo VS120COMNTOOLS environment variable is not found. Check your Visual Studio 2013 installation
    goto Cleanup
)

:Phone8.1ARM
if %BUILD.ARM%==N goto Phone8.1x86
echo Building FFmpeg for Windows Phone 8.1 apps ARM...
echo:

setlocal
call "%VS120COMNTOOLS%..\..\VC\vcvarsall.bat" x86_arm
set LIB=%VSINSTALLDIR%VC\lib\store\ARM;%VSINSTALLDIR%VC\atlmfc\lib\ARM;%WindowsSdkDir%..\..\Windows Phone Kits\8.1\lib\arm;;
set LIBPATH=%VSINSTALLDIR%VC\atlmfc\lib\ARM;%VSINSTALLDIR%VC\lib\ARM
set INCLUDE=%VSINSTALLDIR%VC\include;%VSINSTALLDIR%VC\atlmfc\include;%WindowsSdkDir%..\..\Windows Phone Kits\8.1\Include;%WindowsSdkDir%..\..\Windows Phone Kits\8.1\Include\abi;%WindowsSdkDir%..\..\Windows Phone Kits\8.1\Include\mincore;%WindowsSdkDir%..\..\Windows Phone Kits\8.1\Include\minwin;%WindowsSdkDir%..\..\Windows Phone Kits\8.1\Include\wrl;

%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh Phone8.1 ARM
endlocal

:Phone8.1x86
if %BUILD.x86%==N goto Win7
echo Building FFmpeg for Windows Phone 8.1 apps x86...
echo:

setlocal
call "%VS120COMNTOOLS%..\..\VC\vcvarsall.bat" x86
set LIB=%VSINSTALLDIR%VC\lib\store;%VSINSTALLDIR%VC\atlmfc\lib;%WindowsSdkDir%..\..\Windows Phone Kits\8.1\lib\x86;;
set LIBPATH=%VSINSTALLDIR%VC\atlmfc\lib;%VSINSTALLDIR%VC\lib
set INCLUDE=%VSINSTALLDIR%VC\INCLUDE;%VSINSTALLDIR%VC\ATLMFC\INCLUDE;%WindowsSdkDir%..\..\Windows Phone Kits\8.1\Include;%WindowsSdkDir%..\..\Windows Phone Kits\8.1\Include\abi;%WindowsSdkDir%..\..\Windows Phone Kits\8.1\Include\mincore;%WindowsSdkDir%..\..\Windows Phone Kits\8.1\Include\minwin;%WindowsSdkDir%..\..\Windows Phone Kits\8.1\Include\wrl;

%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh Phone8.1 x86
endlocal

:: Build and deploy Windows 7 library
:Win7
if %BUILD.win7%==N goto Cleanup

:: Check for required tools
if not defined VS120COMNTOOLS (
    echo:
    echo VS120COMNTOOLS environment variable is not found. Check your Visual Studio 2013 installation
    goto Cleanup
)

:Win7x86
if %BUILD.x86%==N goto Win7x64
echo Building FFmpeg for Windows 7 desktop x86...
echo:

setlocal
call "%VS120COMNTOOLS%..\..\VC\vcvarsall.bat" x86
set LIB=%VSINSTALLDIR%VC\lib;%VSINSTALLDIR%VC\atlmfc\lib;%WindowsSdkDir%lib\winv6.3\um\x86;
set LIBPATH=%WindowsSdkDir%References\CommonConfiguration\Neutral;%VSINSTALLDIR%VC\lib;
set INCLUDE=%VSINSTALLDIR%VC\include;%WindowsSdkDir%Include\um;%WindowsSdkDir%Include\shared;

%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh Win7 x86
endlocal

:Win7x64
if %BUILD.x64%==N goto Cleanup
echo Building FFmpeg for Windows 7 desktop x64...
echo:

setlocal
call "%VS120COMNTOOLS%..\..\VC\vcvarsall.bat" x64
set LIB=%VSINSTALLDIR%VC\lib\amd64;%VSINSTALLDIR%VC\atlmfc\lib\amd64;%WindowsSdkDir%lib\winv6.3\um\x64;
set LIBPATH=%WindowsSdkDir%References\CommonConfiguration\Neutral;%VSINSTALLDIR%VC\lib\amd64;
set INCLUDE=%VSINSTALLDIR%VC\include;%WindowsSdkDir%Include\um;%WindowsSdkDir%Include\shared;

%MSYS2_BIN% --login -x %~dp0FFmpegConfig.sh Win7 x64
endlocal

goto Cleanup

:: Display help message
:Usage
echo The correct usage is:
echo:
echo     %0 [target platform] [architecture]
echo:
echo where
echo:
echo [target platform] is: win10 ^| win8.1 ^| phone8.1 ^| win7 (at least one)
echo [architecture]    is: x86 ^| x64 ^| ARM (optional)
echo:
echo For example:
echo     %0 win10                     - Build for Windows 10 ARM, x64, and x86
echo     %0 phone8.1 ARM              - Build for Windows Phone 8.1 ARM only
echo     %0 win8.1 x86 x64            - Build for Windows 8.1 x86 and x64 only
echo     %0 win7 x86 x64              - Build for Windows 7 x86 and x64 only
echo     %0 phone8.1 win10 ARM        - Build for Windows 10 and Windows Phone 8.1 ARM only
echo     %0 win8.1 phone8.1 win10     - Build all architecture for all target platform
goto :eof

:Cleanup
@endlocal