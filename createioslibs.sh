#!/bin/sh

#Based on Tomohiro Matsuzawa SRT build scrips found here -> https://github.com/cats-oss/VideoCast-Swift

SDKVERSION=$(xcrun -sdk iphoneos --show-sdk-version)

#Remove the file libcrypto.a to build a new version
if [[ ! -f libcrypto.a ]]; then
    echo "libcrypto missing. Will try to build one."
    #since SRT depends on this lib remove libsrt.a
    rm -f libsrt.a
    if [ ! -d openssl ]; then
	   mkdir openssl
	   git clone https://github.com/x2on/OpenSSL-for-iPhone ./openssl/
    fi

#here we got the directory openssl and the repo OpenSSL-for-iPhone
#Make sure we-re on latest master-commit //Change this if you want a branch or lock to a certain commit

    cd openssl
    git fetch --all
    git reset --hard origin/master
    ./build-libssl.sh
    cd ..
    cp ./openssl/lib/libcrypto.a .
fi

#Remove the file libsrt.a to build a new version
if [[ ! -f libsrt.a ]]; then
    echo "libsrt missing. Will try to build one."
    if [ ! -d srt ]; then
	   mkdir srt
    	git clone https://github.com/Haivision/srt ./srt/
    fi

#here we got the directory srt and the repo Haivision/srt
#Make sure we-re on latest master-commit //Change this if you want a branch or lock to a certain commit

    cd srt
    git fetch --all
    git reset --hard origin/master
    cd ..

    build_srt() {
      PLATFORM=$1
      IOS_PLATFORM=$2
      ARCH=$3
      IOS_OPENSSL=$(pwd)/openssl/bin/${PLATFORM}${SDKVERSION}-${ARCH}.sdk
      mkdir -p ./build/ios_${ARCH}
      pushd ./build/ios_${ARCH}
      ../../srt/configure --cmake-prefix-path=$IOS_OPENSSL --use-openssl-pc=OFF --cmake-toolchain-file=scripts/iOS.cmake --enable-debug=0 --ios-platform=${IOS_PLATFORM} --ios-arch=${ARCH}
      make
      popd
    }

    build_srt iPhoneSimulator SIMULATOR64 x86_64
    build_srt iPhoneOS OS arm64

    cp ./build/ios_arm64/version.h srt/srtcore/.

    awk '
/^struct CBytePerfMon/ { g = 1 }
p == 1 { print }
g == 1 { print "struct CBytePerfMonExpose"; p = 1; g = 0 }
/};/ { p = 0 }
' srt/srtcore/srt.h > srt_status.h

    lipo -output libsrt.a -create ./build/ios_x86_64/libsrt.a ./build/ios_arm64/libsrt.a
fi