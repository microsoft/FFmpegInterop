#!/bin/bash
dir=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

# Parse the options
options=$(getopt -o "" --long arch:,app-platform:,settings:,crt:,fuzzing-libs: -n "$0" -- "$@")
if [ $? -ne 0 ]; then
    echo "ERROR: Invalid option(s)"
    exit 1
fi

eval set -- "$options"

while true; do
    case "$1" in
        --arch)
            arch=$2
            shift 2
            ;;
        --app-platform)
            case "${2,,}" in
                desktop)
                    app_platform_settings="--extra-cflags=\"-DWINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP\""
                    ;;
                onecore)
                    app_platform_settings=" \
                        --extra-cflags=\"-DWINAPI_FAMILY=WINAPI_FAMILY_APP -D_WIN32_WINNT=0x0A00\" \
                        --extra-ldflags=\"-APPCONTAINER WindowsApp.lib -NODEFAULTLIB:kernel32.lib -DEFAULTLIB:onecore.lib\""
                    ;;
                uwp)
                    app_platform_settings=" \
                        --extra-cflags=\"-DWINAPI_FAMILY=WINAPI_FAMILY_APP -D_WIN32_WINNT=0x0A00\" \
                        --extra-ldflags=\"-APPCONTAINER WindowsApp.lib\""
                    ;;
                *)
                    echo "Error: Invalid value for --app-platform: $2. Expected: desktop, onecore, or uwp"
                    exit 1
                    ;;
            esac
            shift 2
            ;;
        --crt)
            crt=$2
            case "${2,,}" in
                dynamic)
                    crt_settings="--extra-cflags=\"-MD\""
                    ;;
                hybrid)
                    # The hybrid CRT is a technique using the Universal CRT AND the static CRT to get functional coverage
                    # without the overhead of the static CRT or the external dependency of the dynamic CRT.
                    #
                    # Learn more about the hybrid CRT:
                    # - https://github.com/microsoft/WindowsAppSDK/blob/main/docs/Coding-Guidelines/HybridCRT.md
                    # - https://www.youtube.com/watch?v=bNHGU6xmUzE&t=977s
                    crt_settings="--extra-cflags=\"-MT\" --extra-ldflags=\"-NODEFAULTLIB:libucrt.lib -DEFAULTLIB:ucrt.lib\""
                    ;;
                static)
                    crt_settings="--extra-cflags=\"-MT\""
                    ;;
                *)
                    echo "Error: Invalid value for --crt: $2. Expected: dynamic, hybrid, or static"
                    exit 1
                    ;;
            esac
            shift 2
            ;;
        --settings)
            user_settings=$2
            shift 2
            ;;
        --fuzzing-libs)
            fuzzing=true
            fuzzing_libs=$2
            shift 2
            ;;
        --)
            shift
            break
            ;;
        *)
            echo "ERROR: Invalid option: -$1" 1>&2
            exit 1
            ;;
    esac
done

# Common settings
common_settings=" \
    --toolchain=msvc \
    --target-os=win32 \
    --disable-programs \
    --disable-d3d11va \
    --disable-dxva2 \
    --enable-shared \
    --enable-cross-compile \
    --extra-cflags=\"-GUARD:CF -Qspectre -Gy -Gw\" \
    --extra-ldflags=\"-PROFILE -GUARD:CF -DYNAMICBASE -OPT:ICF -OPT:REF\" \
    "

# Architecture-specific settings
if [ -z $arch ]; then
    echo "ERROR: No architecture set" 1>&2
    exit 1
elif [ $arch == "x86" ]; then
    arch_settings="
        --arch=x86 \
        --extra-ldflags=\"-CETCOMPAT\" \
        --prefix=../../Build/$arch \
        "
elif [ $arch == "x64" ]; then
    arch_settings="
        --arch=x86_64 \
        --extra-ldflags=\"-CETCOMPAT\" \
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

# Fuzzer-specific settings
if [[ $fuzzing ]]; then
    if [[ "$arch" == "arm" || "$arch" == "arm64" ]]; then
        echo "ERROR: Fuzzing is not supported on ARM/ARM64 architectures" 1>&2
        exit 1
    fi

    fuzz_settings="
        --extra-cflags=\"-fsanitize=address -fsanitize-coverage=inline-8bit-counters -fsanitize-coverage=edge -fsanitize-coverage=trace-cmp -fsanitize-coverage=trace-div\" \
        --extra-ldflags=\"-LIBPATH:$fuzzing_libs\" \
        "

    # Add sancov.lib or libsancov.lib based on CRT
    if [[ $crt == "dynamic" ]]; then
        fuzz_crt_settings="--extra-ldflags=\"-DEFAULTLIB:sancov.lib\""
    else
        fuzz_crt_settings="--extra-ldflags=\"-DEFAULTLIB:libsancov.lib\""
    fi  
fi


# Build FFmpeg
pushd $dir/ffmpeg > /dev/null

rm -rf Output/$arch
mkdir -p Output/$arch
cd Output/$arch

eval ../../configure $common_settings $arch_settings $app_platform_settings $crt_settings $onecore_settings $user_settings &&
make -j`nproc` &&
make install

result=$?

popd > /dev/null

if [ $result -ne 0 ]; then
    echo "ERROR: FFmpeg build for $arch failed with exit code $result" 1>&2
fi

exit $result
