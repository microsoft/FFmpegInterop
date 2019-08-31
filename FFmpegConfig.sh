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
	--extra-cflags=\"-MD -DWINAPI_FAMILY=WINAPI_FAMILY_APP -D_WIN32_WINNT=0x0A00\" \
    --extra-ldflags=\"-APPCONTAINER WindowsApp.lib\" \
    "

# Architecture specific configure settings
arch="${1,,}"

if [ -z $arch ]; then
	echo "ERROR: No architecture set!" 1>&2
    exit 1
elif [ $arch == "x86" ]; then
	architecture_settings="
		--arch=x86 \
        --prefix=../../Build/$arch \
	"
elif [ $arch == "x64" ]; then
	architecture_settings="
		--arch=x86_64 \
        --prefix=../../Build/$arch \
	"
elif [ $arch == "arm" ]; then
    architecture_settings="
		--arch=arm \
        --as=armasm \
        --cpu=armv7 \
        --enable-thumb \
        --extra-cflags=\"-D__ARM_PCS_VFP\" \
        --prefix=../../Build/$arch \
	"
elif [ $arch == "arm64" ]; then
    architecture_settings="
		--arch=arm64 \
        --as=armasm \
        --cpu=armv7 \
        --enable-thumb \
        --extra-cflags=\"-D__ARM_PCS_VFP\" \
        --prefix=../../Build/$arch \
	"
else
    echo "ERROR: $arch is not a valid architecture!" 1>&2
    exit 1
fi

# Extra configure settings supplied by user
extra_settings="${2:-""}"

# Build FFmpeg
pushd $dir/ffmpeg > /dev/null

rm -rf Output/$arch
mkdir -p Output/$arch
cd Output/$arch

eval ../../configure $common_settings $arch_settings $extra_settings

make -j`nproc`
make install

popd > /dev/null

exit 0
