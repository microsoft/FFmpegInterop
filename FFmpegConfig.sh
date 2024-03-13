#!/bin/bash
dir=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

# Common configure settings for all architectures
common_settings=" \
    --toolchain=msvc \
    --target-os=win32 \
    --disable-programs \
    --disable-d3d11va \
    --disable-dxva2 \
    --enable-shared \
    --enable-cross-compile \
    --extra-cflags=\"-MD -DWINAPI_FAMILY=WINAPI_FAMILY_APP -D_WIN32_WINNT=0x0A00 -GUARD:CF\" \
    --extra-ldflags=\"-APPCONTAINER WindowsApp.lib -PROFILE -GUARD:CF -DYNAMICBASE\" \
    "

# Architecture specific configure settings
arch="${1,,}"

if [ -z $arch ]; then
    echo "ERROR: No architecture set" 1>&2
    exit 1
elif [ $arch == "x86" ]; then
    arch_settings="
        --arch=x86 \
        --prefix=../../Build/$arch \
        "
elif [ $arch == "x64" ]; then
    arch_settings="
        --arch=x86_64 \
        --prefix=../../Build/$arch \
        "
elif [ $arch == "arm" ]; then
    arch_settings="
        --arch=arm \
        --as=armasm \
        --cpu=armv7 \
        --enable-thumb \
        --extra-cflags=\"-D__ARM_PCS_VFP\" \
        --prefix=../../Build/$arch \
        "
elif [ $arch == "arm64" ]; then
    arch_settings="
        --arch=arm64 \
        --as=armasm64 \
        --cpu=armv7 \
        --enable-thumb \
        --extra-cflags=\"-D__ARM_PCS_VFP\" \
        --prefix=../../Build/$arch \
        "
else
    echo "ERROR: $arch is not a valid architecture" 1>&2
    exit 1
fi

# Extra configure settings supplied by user
extra_settings="${2:-""}"

# TODO: Check for UseUCRT flag
# TODO: Clean up multiline strings
# TODO: Remove any unnecessary libs
windows_sdk_lib_path="${WindowsSdkDir}lib\\${WindowsSDKLibVersion}um\\${arch}" # WindowsSDK_LibraryPath_x64=%WindowsSdkDir%lib\%WindowsSDKLibVersion%um\$arch # TODO: Support all architectures
ucrt_lib_path="${UniversalCRTSdkDir}lib\\${UCRTVersion}\\ucrt\\${arch}" # UniversalCRT_LibraryPath_x64=%UniversalCRTSdkDir%lib\%UCRTVersion%\ucrt\$arch # TODO: Support all architectures
vc_lib_path="${VCToolsInstallDir}lib\\onecore\\${arch}" # VC_LibraryPath_VC_x64_OneCore=%VCToolsInstallDir%lib\onecore\$arch # TODO: Support all architectures
ucrt_dll_path="${UniversalCRTSdkDir}redist\\${UCRTVersion}\\ucrt\\DLLs\\${arch}" # UniversalCRT_LibraryPath_x64=%UniversalCRTSdkDir%lib\%UCRTVersion%\ucrt\$arch # TODO: Support all architectures
ucrt_base_dll="ucrtbase.dll" # TODO: -LIBPATH: or no flag? Support debug? 
additional_dependencies="onecore_apiset.lib onecoreuap_apiset.lib"
additional_dependencies+="ucrt.lib libvcruntime.lib msvcrt.lib msvcprt.lib" # TODO: Support debug?

# TODO: Remove -VERBOSE
common_settings+="\
    --extra-ldflags='-VERBOSE -NODEFAULTLIB -LIBPATH:\"$vc_lib_path\" \
        -LIBPATH:\"$windows_sdk_lib_path\" \
        -LIBPATH:\"$ucrt_lib_path\" \
        -LIBPATH:\"$vc_lib_path\" \
        $additional_dependencies' \
    "

# Build FFmpeg
pushd $dir/ffmpeg > /dev/null

rm -rf Output/$arch
mkdir -p Output/$arch
cd Output/$arch

eval ../../configure $common_settings $arch_settings $extra_settings &&
make -j`nproc` &&
make install

result=$?

popd > /dev/null

if [ $result -ne 0 ]; then
    echo "ERROR: FFmpeg build for $arch failed with exit code $result" 1>&2
fi

# TODO: Uncomment
# exit $result
