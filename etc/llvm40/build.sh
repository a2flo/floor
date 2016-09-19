#!/bin/bash

PRERELEASE=
BASE_RELEASE=4.0.0
RELEASE=${BASE_RELEASE}${PRERELEASE}
VERSION_NAME=40000

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

# clean up prior build folders
rm -Rf build 2>/dev/null

# download src
LLVM_COMMIT=bde752e10f1536a10392e82aa0a24032f3c19923
CLANG_COMMIT=add6a13495efd9fc81ac959158299a27b1c29dd0
LIBCXX_COMMIT=eeae30ead0fedb3f34a1d7bcc70f7004682cf24e

if [ ! -d llvm ]; then
	# init llvm/clang repositories if they don't exist yet
	mkdir -p llvm/tools/clang

	cd llvm
	git init
	git remote add origin https://github.com/llvm-mirror/llvm.git
	git fetch --depth=2500 origin master
	git reset --hard ${LLVM_COMMIT}

	cd tools/clang
	git init
	git remote add origin https://github.com/llvm-mirror/clang.git
	git fetch --depth=2500 origin master
	git reset --hard ${CLANG_COMMIT}
else
	# already exists, just need to clean+reset and possibly update/fetch if head revision changed
	cd llvm
	git clean -f -d
	CURRENT_LLVM_COMMIT=$(git rev-parse HEAD)
	if [ ${CURRENT_LLVM_COMMIT} != ${LLVM_COMMIT} ]; then
		git reset --hard ${CURRENT_LLVM_COMMIT}
		git fetch --depth=2500 origin master
	fi
	git reset --hard ${LLVM_COMMIT}

	cd tools/clang
	git clean -f -d
	CURRENT_CLANG_COMMIT=$(git rev-parse HEAD)
	if [ ${CURRENT_CLANG_COMMIT} != ${CLANG_COMMIT} ]; then
		git reset --hard ${CURRENT_CLANG_COMMIT}
		git fetch --depth=2500 origin master
	fi
	git reset --hard ${CLANG_COMMIT}
fi
cd ../../../

if [ ! -d libcxx ]; then
	mkdir libcxx
	cd libcxx
	git init
	git remote add origin https://github.com/llvm-mirror/libcxx.git
	git fetch --depth=1000 origin master
	git reset --hard ${LIBCXX_COMMIT}
else
	cd libcxx
	git clean -f -d
	CURRENT_LIBCXX_COMMIT=$(git rev-parse HEAD)
	if [ ${CURRENT_LIBCXX_COMMIT} != ${LIBCXX_COMMIT} ]; then
		git reset --hard ${CURRENT_LIBCXX_COMMIT}
		git fetch --depth=1000 origin master
	fi
	git reset --hard ${LIBCXX_COMMIT}
fi
cd ..

# always clone anew
rm -Rf SPIRV-Tools 2>/dev/null
git clone git://github.com/a2flo/SPIRV-Tools.git
git clone git://github.com/KhronosGroup/SPIRV-Headers.git SPIRV-Tools/external/spirv-headers
cd SPIRV-Tools/external/spirv-headers
git reset --hard e4e22fd832810c45f46d542c67910b0c6bf4ca2d
cd ../../../

# patch
cd llvm
git apply -p1 --ignore-whitespace ../40_llvm.patch
cd tools/clang
git apply -p1 --ignore-whitespace ../../../40_clang.patch
cd ../../../libcxx
git apply -p1 --ignore-whitespace ../40_libcxx.patch
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

# TODO: cmake!
EXTRA_OPTIONS=""
if [ $BUILD_OS == "osx" ]; then
	EXTRA_OPTIONS="-mmacosx-version-min=10.11"
elif [ $BUILD_OS == "ios" ]; then
	EXTRA_OPTIONS="-miphoneos-version-min=9.0"
fi

CONFIG_OPTIONS=
CLANG_OPTIONS=
if [ $CXX == "clang++" ]; then
	# TODO: cmake!
	# LLVM_ENABLE_LIBCXX LLVM_ENABLE_LIBCXXABI
	CONFIG_OPTIONS="--enable-libcpp"
	CLANG_OPTIONS="-fvectorize"
	if [ $ENABLE_LTO -gt 0 ]; then
		CLANG_OPTIONS="${CLANG_OPTIONS} -flto"
	fi
fi
if [ $BUILD_OS == "linux" ]; then
	# TODO: add to cmake if necessary?
	CONFIG_OPTIONS="${CONFIG_OPTIONS} --prefix=/usr --sysconfdir=/etc --with-binutils-include=/usr/include --with-python=/usr/bin/python2"
fi

# TODO: LLVM_TOOL_LLI_BUILD LLVM_TOOL_LLGO_BUILD LLVM_TOOL_LLVM_GO_BUILD
# TODO: polly?
CC=${CC} CXX=${CXX} cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS_RELEASE="-Ofast -msse4.1 -mtune=native -funroll-loops -DNDEBUG" -DCMAKE_CXX_FLAGS_RELEASE="-Ofast -msse4.1 -mtune=native -funroll-loops -DNDEBUG" -DLLVM_TARGETS_TO_BUILD="X86;NVPTX" -DLLVM_BUILD_EXAMPLES=OFF -DLLVM_INCLUDE_EXAMPLES=OFF -DLLVM_BUILD_TESTS=OFF -DLLVM_INCLUDE_TESTS=OFF -DLLVM_ENABLE_CXX1Y=ON -DLLVM_ENABLE_ASSERTIONS=OFF -DLLVM_ENABLE_FFI=OFF -DLLVM_BUILD_DOCS=OFF -DLLVM_ABI_BREAKING_CHECKS=FORCE_OFF -DLLVM_BINDINGS_LIST= ../llvm
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
