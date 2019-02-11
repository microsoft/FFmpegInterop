param (
    [ValidateSet('x64', 'x86', 'ARM')]
    [string] $Platform = 'x64',

    [string] $MSys2Dir = 'C:\msys64',

    [string] $VSInstallDir = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2017\Enterprise",
    [string] $VCVersion = '14.16.27023',
    [string] $VCInstallDir = "$VSInstallDir\VC\Tools\MSVC\$VCVersion",

    [string] $WindowsKitsDir = "${env:ProgramFiles(x86)}\Windows Kits",
    [string] $UniversalCRTSdkDir = "$WindowsKitsDir\10",
    [string] $UCRTVersion = '10.0.17763.0',
    [string] $NetFxVersion = '4.7.2'
)

$env:LIB =  "$VCInstallDir\lib\$Platform\store;" +
            "$VCInstallDir\atlmfc\lib\$Platform;" +
            "$UniversalCRTSdkDir\lib\$UCRTVersion\ucrt\$Platform;" +
            "$UniversalCRTSdkDir\lib\$UCRTVersion\um\$Platform;" +
            "$WindowsKitsDir\NETFXSDK\$NetFxVersion\Lib\um\$Platform"

$env:LIBPATH =  "$VCInstallDir\atlmfc\lib\$Platform;" +
                "$VCInstallDir\lib\$Platform"

$env:INCLUDE =  "$VCInstallDir\include;" +
                "$UniversalCRTSdkDir\Include\$UCRTVersion\ucrt;" +
                "$UniversalCRTSdkDir\Include\$UCRTVersion\um;" +
                "$UniversalCRTSdkDir\Include\$UCRTVersion\shared;" +
                "$UniversalCRTSdkDir\Include\$UCRTVersion\winrt;" +
                "$WindowsKitsDir\NETFXSDK\$NetFxVersion\Include\um"

#TODO: Use VCVars instead?
#$vcvarsall = "$VSInstallDir\VC\Auxiliary\Build\vcvarsall.bat" # Not currently used.

$bash = "$MSys2Dir\usr\bin\bash.exe"
& $bash --login -x $PSScriptRoot\FFmpegConfig.sh Win10 $Platform
