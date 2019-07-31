#!/bin/bash
DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )


if [ "$1" == "x86" ]; then
    echo "Make x86"
    pushd $DIR/ffmpeg
    rm -rf Output/x86
    mkdir -p Output/x86
    cd Output/x86
    ../../configure \
    --toolchain=msvc \
    --disable-programs \
    --disable-d3d11va \
    --disable-dxva2 \
    --arch=x86 \
    --enable-shared \
    --enable-cross-compile \
    --target-os=win32 \
    --extra-cflags="-MD -DWINAPI_FAMILY=WINAPI_FAMILY_APP -D_WIN32_WINNT=0x0A00" \
    --extra-ldflags="-APPCONTAINER WindowsApp.lib" \
    --prefix=../../Build/x86
    make -j`nproc`
    make install
    popd

elif [ "$1" == "x64" ]; then
    echo "Make x64"
    pushd $DIR/ffmpeg
    rm -rf Output/x64
    mkdir -p Output/x64
    cd Output/x64
    ../../configure \
    --toolchain=msvc \
    --disable-programs \
    --disable-d3d11va \
    --disable-dxva2 \
    --arch=x86_64 \
    --enable-shared \
    --enable-cross-compile \
    --target-os=win32 \
    --extra-cflags="-MD -DWINAPI_FAMILY=WINAPI_FAMILY_APP -D_WIN32_WINNT=0x0A00" \
    --extra-ldflags="-APPCONTAINER WindowsApp.lib" \
    --prefix=../../Build/x64
    make -j`nproc`
    make install
    popd

elif [ "$1" == "ARM" ]; then
    echo "Make ARM"
    pushd $DIR/ffmpeg
    rm -rf Output/ARM
    mkdir -p Output/ARM
    cd Output/ARM
    ../../configure \
    --toolchain=msvc \
    --disable-programs \
    --disable-d3d11va \
    --disable-dxva2 \
    --arch=arm \
    --as=armasm \
    --cpu=armv7 \
    --enable-thumb \
    --enable-shared \
    --enable-cross-compile \
    --target-os=win32 \
    --extra-cflags="-MD -DWINAPI_FAMILY=WINAPI_FAMILY_APP -D_WIN32_WINNT=0x0A00 -D__ARM_PCS_VFP" \
    --extra-ldflags="-APPCONTAINER WindowsApp.lib" \
    --prefix=../../Build/ARM
    make -j`nproc`
    make install
    popd

fi

exit 0