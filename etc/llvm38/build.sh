#!/bin/bash

PRERELEASE=
BASE_RELEASE=3.8.1
RELEASE=${BASE_RELEASE}${PRERELEASE}
VERSION_NAME=30801

##########################################
# helper functions
error() {
	printf "\033[1;31m>> $@ \033[m\n"
	exit 1
}
warning() {
	printf "\033[1;33m>> $@ \033[m\n"
}
info() {
	printf "\033[1;32m>> $@ \033[m\n"
}
verbose() {
	if [ ${BUILD_VERBOSE} -gt 0 ]; then
		printf "$@\n"
	fi
}

##########################################
# build

# handle args
BUILD_JOB_COUNT=0
ENABLE_LTO=0
for arg in "$@"; do
	case $arg in
		# NOTE: this overwrites the detected job/cpu count
		"-j"*)
			BUILD_JOB_COUNT=$(echo $arg | cut -c 3-)
			if [ -z ${BUILD_JOB_COUNT} ]; then
				BUILD_JOB_COUNT=0
			fi
			;;
		"-lto")
			ENABLE_LTO=1
			;;
		*)
			warning "unknown argument: ${arg}"
			;;
	esac
done

# download src archives (if not already done)
BASE_URL=releases/${RELEASE}/
if [ ${PRERELEASE} ]; then
	BASE_URL=pre-releases/${BASE_RELEASE}/${PRERELEASE}/
fi

if [ ! -f llvm-${RELEASE}.src.tar.xz ]; then
	curl -o llvm-${RELEASE}.src.tar.xz http://llvm.org/${BASE_URL}/llvm-${RELEASE}.src.tar.xz
fi
if [ ! -f cfe-${RELEASE}.src.tar.xz ]; then
	curl -o cfe-${RELEASE}.src.tar.xz http://llvm.org/${BASE_URL}/cfe-${RELEASE}.src.tar.xz
fi
if [ ! -f libcxx-${RELEASE}.src.tar.xz ]; then
	curl -o libcxx-${RELEASE}.src.tar.xz http://llvm.org/${BASE_URL}/libcxx-${RELEASE}.src.tar.xz
fi

# always clone anew
rm -Rf SPIRV-Tools 2>/dev/null
git clone git://github.com/a2flo/SPIRV-Tools.git
git clone git://github.com/KhronosGroup/SPIRV-Headers.git SPIRV-Tools/external/SPIRV-Headers

# clean up prior source and build folders
rm -Rf llvm 2>/dev/null
rm -Rf libcxx 2>/dev/null
rm -Rf build 2>/dev/null

# untar and move
tar xfv llvm-${RELEASE}.src.tar.xz
tar xfv cfe-${RELEASE}.src.tar.xz
tar xfv libcxx-${RELEASE}.src.tar.xz

mv llvm-${RELEASE}.src llvm
mv cfe-${RELEASE}.src llvm/tools/clang
mv libcxx-${RELEASE}.src libcxx

# patch
cd llvm
patch -p1 < ../381_llvm.patch
cd tools/clang
patch -p1 < ../../../381_clang.patch
cd ../../../libcxx
patch -p1 < ../381_libcxx.patch
cd ..

# handle platform
BUILD_PLATFORM=$(uname | tr [:upper:] [:lower:])
BUILD_OS="unknown"
BUILD_CPU_COUNT=1
case ${BUILD_PLATFORM} in
	"darwin")
		if expr `uname -p` : "arm.*" >/dev/null; then
			BUILD_OS="ios"
		else
			BUILD_OS="osx"
		fi
		BUILD_CPU_COUNT=$(sysctl -n hw.ncpu)
		export CXX=clang++
		export CC=clang
		;;
	"linux")
		BUILD_OS="linux"
		# note that this includes hyper-threading and multi-socket systems
		BUILD_CPU_COUNT=$(cat /proc/cpuinfo | grep "processor" | wc -l)
		export CXX=g++
		export CC=gcc
		;;
	"freebsd")
		BUILD_OS="freebsd"
		BUILD_CPU_COUNT=$(sysctl -n hw.ncpu)
		export CXX=clang++
		export CC=clang
		;;
	"openbsd")
		BUILD_OS="openbsd"
		BUILD_CPU_COUNT=$(sysctl -n hw.ncpu)
		export CXX=eg++
		export CC=egcc
		;;
	"mingw"*)
		BUILD_OS="mingw"
		BUILD_CPU_COUNT=$(env | grep 'NUMBER_OF_PROCESSORS' | sed -E 's/.*=([:digit:]*)/\1/g')
		export CXX=g++
		export CC=gcc
		;;
	*)
		warning "unknown build platform - trying to continue! ${BUILD_PLATFORM}"
		;;
esac

# if job count is unspecified (0), set it to #cpus
if [ ${BUILD_JOB_COUNT} -eq 0 ]; then
	BUILD_JOB_COUNT=${BUILD_CPU_COUNT}
fi
info "using ${BUILD_JOB_COUNT} build jobs"

# build spir-v tools
cd SPIRV-Tools
mkdir build
cd build
cmake -G "Unix Makefiles" -DUNIX=1 -DCMAKE_BUILD_TYPE=Release ../
make -j ${BUILD_JOB_COUNT}
make_ret_code=$?

if [ ${make_ret_code} -ne 0 ]; then
	error "toolchain compilation failed!"
fi

cd ../../

# build clang/llvm
mkdir build
cd build

EXTRA_OPTIONS=""
if [ $BUILD_OS == "osx" ]; then
	EXTRA_OPTIONS="-mmacosx-version-min=10.9"
elif [ $BUILD_OS == "ios" ]; then
	EXTRA_OPTIONS="-miphoneos-version-min=9.0"
fi

CONFIG_OPTIONS=
CLANG_OPTIONS=
if [ $CXX == "clang++" ]; then
	CONFIG_OPTIONS="--enable-libcpp"
	CLANG_OPTIONS="-fvectorize"
	if [ $ENABLE_LTO -gt 0 ]; then
		CLANG_OPTIONS="${CLANG_OPTIONS} -flto"
	fi
fi
if [ $BUILD_OS == "linux" ]; then
	CONFIG_OPTIONS="${CONFIG_OPTIONS} --prefix=/usr --sysconfdir=/etc --with-binutils-include=/usr/include --with-python=/usr/bin/python2"
fi

# NOTE: not using cmake here, because there are still issues with it right now
CC=${CC} CXX=${CXX} ../llvm/configure ${CONFIG_OPTIONS} --enable-optimized --disable-assertions --enable-cxx11 --enable-targets="host,nvptx" --disable-docs --disable-jit --disable-bindings --with-optimize-option="-Ofast -msse4.1 ${CLANG_OPTIONS} -funroll-loops -mtune=native" --with-extra-options="${EXTRA_OPTIONS}"
make -j ${BUILD_JOB_COUNT}
make_ret_code=$?

# get out of the "build" folder
cd ..
if [ ${make_ret_code} -ne 0 ]; then
	error "toolchain compilation failed!"
else
	info ""
	info "#########################"
	info "# clang/llvm build done #"
	info "#########################"
	info ""
fi
