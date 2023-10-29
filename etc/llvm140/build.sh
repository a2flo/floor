#!/usr/bin/env bash

PRERELEASE=
BASE_RELEASE=14.0.6
RELEASE=${BASE_RELEASE}${PRERELEASE}
VERSION_NAME=140006

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
LLVM_REV=36aaba48c6c9fdbd33dffe15d21baccf17ac00d7
LLVM_SPIRV_REV=249ea927574386772650d671a4ea3b41538b2cc0
if [ ! -d llvm ]; then
	git clone -b floor_toolchain_1406 https://github.com/a2flo/floor_llvm.git llvm
	cd llvm
	git submodule init
	git submodule update
else
	# already exists, just need to clean+reset and pull
	cd llvm
	git clean -f -d
	git reset --hard
	git pull
	git reset --hard ${LLVM_REV}
	if [ $? -ne 0 ]; then
		error "expected LLVM rev/commit doesn't exist"
	fi
	
	cd llvm/projects/spirv
	git clean -f -d
	git reset --hard
	git checkout floor_toolchain_1406
	cd ../
	git submodule update
	cd spirv/
	git reset --hard ${LLVM_SPIRV_REV}
	if [ $? -ne 0 ]; then
		error "expected LLVM-SPIRV rev/commit doesn't exist"
	fi
	
	cd ../../../
fi
cd ../

# always clone anew
rm -Rf SPIRV-Tools 2>/dev/null
git clone -b libfloor_202305 https://github.com/a2flo/SPIRV-Tools.git
git clone -b main https://github.com/KhronosGroup/SPIRV-Headers.git SPIRV-Tools/external/spirv-headers
cd SPIRV-Tools/external/spirv-headers
git reset --hard 268a061764ee69f09a477a695bf6a11ffe311b8d
cd ../../../

# handle platform
BUILD_PLATFORM=$(uname | tr [:upper:] [:lower:])
BUILD_OS="unknown"
BUILD_CPU_COUNT=1
BUILD_CXX=
BUILD_CC=
PLATFORM_OPTIONS=
case ${BUILD_PLATFORM} in
	"darwin")
		if expr `sw_vers -productName` : "macOS" >/dev/null; then
			BUILD_OS="macos"
		else
			BUILD_OS="ios"
		fi
		BUILD_CPU_COUNT=$(sysctl -n hw.ncpu)
		BUILD_CXX=clang++
		BUILD_CC=clang
		;;
	"linux")
		BUILD_OS="linux"
		# note that this includes hyper-threading and multi-socket systems
		BUILD_CPU_COUNT=$(cat /proc/cpuinfo | grep "processor" | wc -l)
		BUILD_CXX=clang++
		BUILD_CC=clang
		;;
	"freebsd")
		BUILD_OS="freebsd"
		BUILD_CPU_COUNT=$(sysctl -n hw.ncpu)
		BUILD_CXX=clang++
		BUILD_CC=clang
		;;
	"openbsd")
		BUILD_OS="openbsd"
		BUILD_CPU_COUNT=$(sysctl -n hw.ncpu)
		BUILD_CXX=eg++
		BUILD_CC=egcc
		;;
	"mingw"*)
		BUILD_OS="mingw"
		BUILD_CPU_COUNT=$(env | grep 'NUMBER_OF_PROCESSORS' | sed -E 's/.*=([[:digit:]]*)/\1/g')
		BUILD_CXX=clang++
		BUILD_CC=clang
		PLATFORM_OPTIONS="-mcmodel=medium"
		;;
	*)
		warning "unknown build platform - trying to continue! ${BUILD_PLATFORM}"
		;;
esac

CPU_OPTIONS="-msse4.1 -mtune=native"
APPLE_DEPLOYMENT_TARGET=
APPLE_ARCHS=
if [ $BUILD_OS == "macos" ]; then
	APPLE_DEPLOYMENT_TARGET="-DCMAKE_OSX_DEPLOYMENT_TARGET=10.13"
	APPLE_ARCHS="-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64"
	CPU_OPTIONS=""
elif [ $BUILD_OS == "ios" ]; then
	APPLE_DEPLOYMENT_TARGET="-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DCMAKE_SYSTEM_NAME=iOS"
	APPLE_ARCHS="-DCMAKE_OSX_ARCHITECTURES=arm64"
	CPU_OPTIONS=""
fi

# if no CXX is set, use a platform specific default (determined above)
if [ -z "${CXX}" ]; then
	export CXX=${BUILD_CXX}
	export CC=${BUILD_CC}
fi

# if job count is unspecified (0), set it to #cpus
if [ ${BUILD_JOB_COUNT} -eq 0 ]; then
	BUILD_JOB_COUNT=${BUILD_CPU_COUNT}
fi
info "using ${BUILD_JOB_COUNT} build jobs"

# build spir-v tools
cd SPIRV-Tools
mkdir build
cd build
cmake -G "Unix Makefiles" -DUNIX=1 -DCMAKE_BUILD_TYPE=Release -DSPIRV_WERROR=OFF -DSPIRV_SKIP_TESTS=ON -DSPIRV_TOOLS_BUILD_STATIC=ON -DBUILD_SHARED_LIBS=0 ${APPLE_DEPLOYMENT_TARGET} ${APPLE_ARCHS} ../
# only build what we need
make -j ${BUILD_JOB_COUNT} spirv-as spirv-cfg spirv-dis spirv-opt spirv-val
make_ret_code=$?

if [ ${make_ret_code} -ne 0 ]; then
	error "toolchain compilation failed!"
fi

cd ../../

# build clang/llvm
mkdir build
cd build

CLANG_OPTIONS=
if [ $CXX == "clang++" ]; then
	info "using clang"
	CLANG_OPTIONS="-fvectorize"
	if [ $ENABLE_LTO -gt 0 ]; then
		CLANG_OPTIONS="${CLANG_OPTIONS} -flto"
	fi
fi

# only build what we need
toolchain_binaries=(clang llvm-as llvm-dis llvm-spirv metallib-dis)

CC=${CC} CXX=${CXX} cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DCMAKE_C_FLAGS_RELEASE="-Ofast -funroll-loops -DNDEBUG -Wno-unknown-warning-option -Wno-deprecated-anon-enum-enum-conversion -Wno-ambiguous-reversed-operator -Wno-deprecated-enum-enum-conversion ${CPU_OPTIONS} ${PLATFORM_OPTIONS} ${CLANG_OPTIONS}" -DCMAKE_CXX_FLAGS_RELEASE="-Ofast -funroll-loops -DNDEBUG -Wno-unknown-warning-option -Wno-deprecated-anon-enum-enum-conversion -Wno-ambiguous-reversed-operator -Wno-deprecated-enum-enum-conversion ${CPU_OPTIONS} ${PLATFORM_OPTIONS} ${CLANG_OPTIONS}" -DLLVM_TARGETS_TO_BUILD="X86;AArch64;NVPTX" -DLLVM_BUILD_EXAMPLES=OFF -DLLVM_INCLUDE_EXAMPLES=OFF -DLLVM_BUILD_TESTS=OFF -DLLVM_INCLUDE_TESTS=OFF -DLLVM_ENABLE_ASSERTIONS=OFF -DLLVM_ENABLE_FFI=OFF -DLLVM_BUILD_DOCS=OFF -DLLVM_ABI_BREAKING_CHECKS=FORCE_OFF -DLLVM_BINDINGS_LIST= -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld" -DLLVM_EXTERNAL_SPIRV_HEADERS_SOURCE_DIR="include" ${APPLE_DEPLOYMENT_TARGET} ${APPLE_ARCHS} ../llvm/llvm
make -j ${BUILD_JOB_COUNT} ${toolchain_binaries[@]}
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
