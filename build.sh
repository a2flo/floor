#!/usr/bin/env bash

##########################################
# helper functions
error() {
	if [ -z "$NO_COLOR" ]; then
		printf "\033[1;31m>> $@ \033[m\n"
	else
		printf "error: $@\n"
	fi
	exit 1
}
warning() {
	if [ -z "$NO_COLOR" ]; then
		printf "\033[1;33m>> $@ \033[m\n"
	else
		printf "warning: $@\n"
	fi
}
info() {
	if [ -z "$NO_COLOR" ]; then
		printf "\033[1;32m>> $@ \033[m\n"
	else
		printf ">> $@\n"
	fi
}
verbose() {
	if [ ${BUILD_VERBOSE} -gt 0 ]; then
		printf "$@\n"
	fi
}

##########################################
# compiler setup/check

# if no CXX/CC are specified, try using clang++/clang
if [ -z "${CXX}" ]; then
	# try using clang++ directly (avoid any nasty wrappers)
	if [[ -n $(command -v /usr/bin/clang++) ]]; then
		CXX=/usr/bin/clang++
	elif [[ -n $(command -v /usr/local/bin/clang++) ]]; then
		CXX=/usr/local/bin/clang++
	else
		CXX=clang++
	fi
fi
command -v ${CXX} >/dev/null 2>&1 || error "clang++ binary not found, please set CXX to a valid clang++ binary"

if [ -z "${CC}" ]; then
	# try using clang directly (avoid any nasty wrappers)
	if [[ -n $(command -v /usr/bin/clang) ]]; then
		CC=/usr/bin/clang
	elif [[ -n $(command -v /usr/local/bin/clang) ]]; then
		CC=/usr/local/bin/clang
	else
		CC=clang
	fi
fi
command -v ${CC} >/dev/null 2>&1 || error "clang binary not found, please set CC to a valid clang binary"

# check if clang is the compiler, fail if not
CXX_VERSION=$(${CXX} -v 2>&1)
if expr "${CXX_VERSION}" : ".*clang" >/dev/null; then
	# also check the clang version
	eval $(${CXX} -E -dM - < /dev/null 2>&1 | grep -E "clang_major|clang_minor|clang_patchlevel" | tr [:lower:] [:upper:] | sed -E "s/.*DEFINE __(.*)__ [\"]*([^ \"]*)[\"]*/export \1=\2/g")
	if expr "${CXX_VERSION}" : "Apple.*" >/dev/null; then
		# apple xcode/llvm/clang versioning scheme -> at least 15.0 is required (ships with Xcode / CLI tools 15.0)
		if [ $CLANG_MAJOR -lt 15 ] || [ $CLANG_MAJOR -eq 15 -a $CLANG_MINOR -lt 0 -a $CLANG_PATCHLEVEL -lt 0 ]; then
			error "at least Xcode 15.0 / Apple clang/LLVM 15.0.0 is required to compile this project!"
		fi
	else
		# standard clang versioning scheme -> at least 16.0 is required
		if [ $CLANG_MAJOR -lt 16 ] || [ $CLANG_MAJOR -eq 16 -a $CLANG_MINOR -lt 0 ]; then
			error "at least clang 16.0 is required to compile this project!"
		fi
	fi
else
	error "only clang is currently supported - please set CXX/CC to clang++/clang and try again!"
fi

##########################################
# arg handling
BUILD_MODE="release"
BUILD_CLEAN=0
BUILD_REBUILD=0
BUILD_STATIC=0
BUILD_JSON=0
BUILD_VERBOSE=0
BUILD_DIR_ROOT="build"
BUILD_JOB_COUNT=0

BUILD_CONF_OPENCL=1
BUILD_CONF_CUDA=1
BUILD_CONF_HOST_COMPUTE=1
BUILD_CONF_METAL=1
BUILD_CONF_VULKAN=1
BUILD_CONF_OPENVR=1
BUILD_CONF_OPENXR=1
BUILD_CONF_LIBSTDCXX=0
BUILD_CONF_NATIVE=0

BUILD_CONF_SANITIZERS=0
BUILD_CONF_ASAN=0
BUILD_CONF_MSAN=0
BUILD_CONF_TSAN=0
BUILD_CONF_UBSAN=0

BUILD_ARCH_SIZE="x64"
BUILD_ARCH=$(${CC} -dumpmachine | sed "s/-.*//")
case $BUILD_ARCH in
	"i386"|"i486"|"i586"|"i686"|"arm7"*|"armv7"*)
		error "32-bit builds are not supported"
		;;
	"x86_64"|"amd64"|"arm64")
		BUILD_ARCH_SIZE="x64"
		;;
	*)
		warning "unknown architecture (${BUILD_ARCH}) - using ${BUILD_ARCH_SIZE}!"
		;;
esac

for arg in "$@"; do
	case $arg in
		"help"|"-help"|"--help")
			info "build script usage:"
			echo ""
			echo "build mode options:"
			echo "	<default>          builds this project in release mode"
			echo "	opt                builds this project in release mode + additional optimizations that take longer to compile (lto)"
			echo "	debug              builds this project in debug mode"
			echo "	rebuild            rebuild all source files of this project"
			echo "	clean              cleans all build binaries and intermediate build files"
			echo "	static             also build a static library next to the dynamic library that is being build"
			echo "	json               creates a compile_commands.json file for use with clang tools"
			echo ""
			echo "build configuration:"
			echo "	no-opencl          disables opencl support"
			echo "	no-cuda            disables cuda support"
			echo "	no-host-compute    disables host compute support"
			echo "	no-metal           disables metal support (default for non-iOS and non-macOS targets)"
			echo "	no-vulkan          disables vulkan support"
			echo "	no-openvr          disables OpenVR support (default for macOS and iOS targets)"
			echo "	no-openxr          disables OpenXR support (default for macOS and iOS targets)"
			echo "	libstdc++          use libstdc++ instead of libc++ (highly discouraged unless building on mingw)"
			echo "	native             optimize and specifically build for the host cpu"
			echo ""
			echo "sanitizers:"
			echo "	asan               build with address sanitizer"
			echo "	msan               build with memory sanitizer"
			echo "	tsan               build with thread sanitizer"
			echo "	ubsan              build with undefined behavior sanitizer"
			echo ""
			echo "misc flags:"
			echo "	-v                 verbose output (prints all executed compiler and linker commands, and some other information)"
			echo "	-vv                very verbose output (same as -v + runs all compiler and linker commands with -v)"
			echo "	-j#                explicitly use # amount of build jobs (instead of automatically using #logical-cpus jobs)"
			echo "	-build-dir=<dir>   sets the build directory for temporary files to the specified <dir> (defaults to 'build')"
			echo ""
			echo ""
			echo "example:"
			echo "	./build.sh -v debug no-cuda -j1"
			echo ""
			exit 0
			;;
		"opt")
			BUILD_MODE="release_opt"
			;;
		"debug")
			BUILD_MODE="debug"
			;;
		"rebuild")
			BUILD_REBUILD=1
			;;
		"static")
			BUILD_STATIC=1
			;;
		"json")
			BUILD_JSON=1
			;;
		"clean")
			BUILD_CLEAN=1
			;;
		"-v")
			BUILD_VERBOSE=1
			;;
		"-vv")
			BUILD_VERBOSE=2
			;;
		"-j"*)
			BUILD_JOB_COUNT=$(echo $arg | cut -c 3-)
			if [ -z ${BUILD_JOB_COUNT} ]; then
				BUILD_JOB_COUNT=0
			fi
			;;
		"-build-dir="*)
			BUILD_DIR_ROOT=$(echo $arg | cut -c 12-)
			;;
		"no-opencl")
			BUILD_CONF_OPENCL=0
			;;
		"no-cuda")
			BUILD_CONF_CUDA=0
			;;
		"no-host-compute")
			BUILD_CONF_HOST_COMPUTE=0
			;;
		"no-metal")
			BUILD_CONF_METAL=0
			;;
		"no-vulkan")
			BUILD_CONF_VULKAN=0
			;;
		"no-openvr")
			BUILD_CONF_OPENVR=0
			;;
		"no-openxr")
			BUILD_CONF_OPENXR=0
			;;
		"libstdc++")
			BUILD_CONF_LIBSTDCXX=1
			;;
		"native")
			BUILD_CONF_NATIVE=1
			;;
		"asan")
			BUILD_CONF_SANITIZERS=1
			BUILD_CONF_ASAN=1
			;;
		"msan")
			BUILD_CONF_SANITIZERS=1
			BUILD_CONF_MSAN=1
			;;
		"tsan")
			BUILD_CONF_SANITIZERS=1
			BUILD_CONF_TSAN=1
			;;
		"ubsan")
			BUILD_CONF_SANITIZERS=1
			BUILD_CONF_UBSAN=1
			;;
		*)
			warning "unknown argument: ${arg}"
			;;
	esac
done

##########################################
# target and build environment setup

# name of the target (part of the binary name)
TARGET_NAME=floor

# check on which platform we're compiling + check how many h/w threads can be used (logical cpus)
BUILD_PLATFORM=$(uname | tr [:upper:] [:lower:])
BUILD_OS="unknown"
BUILD_CPU_COUNT=1
STAT_IS_BSD=0
case ${BUILD_PLATFORM} in
	"darwin")
		if expr `uname -p` : "arm.*" >/dev/null; then
			if expr `sw_vers -productName` : "macOS" >/dev/null; then
				BUILD_OS="macos"
			else
				BUILD_OS="ios"
			fi
		else
			BUILD_OS="macos"
		fi
		BUILD_CPU_COUNT=$(sysctl -n hw.ncpu)
		STAT_IS_BSD=1
		;;
	"linux")
		BUILD_OS="linux"
		# note that this includes hyper-threading and multi-socket systems
		BUILD_CPU_COUNT=$(cat /proc/cpuinfo | grep "processor" | wc -l)
		;;
	"freebsd")
		BUILD_OS="freebsd"
		BUILD_CPU_COUNT=$(sysctl -n hw.ncpu)
		STAT_IS_BSD=1
		;;
	"openbsd")
		BUILD_OS="openbsd"
		BUILD_CPU_COUNT=$(sysctl -n hw.ncpu)
		STAT_IS_BSD=1
		;;
	"cygwin"*)
		# untested
		BUILD_OS="cygwin"
		BUILD_CPU_COUNT=$(env | grep 'NUMBER_OF_PROCESSORS' | sed -E 's/.*=([[:digit:]]*)/\1/g')
		warning "cygwin support is untested and unsupported!"
		;;
	"mingw"*)
		BUILD_OS="mingw"
		BUILD_CPU_COUNT=$(env | grep 'NUMBER_OF_PROCESSORS' | sed -E 's/.*=([[:digit:]]*)/\1/g')
		;;
	*)
		warning "unknown build platform - trying to continue! ${BUILD_PLATFORM}"
		;;
esac

# runs the platform specific stat cmd to get the modification date of the specified file(s) as a unix timestamp
file_mod_time() {
	# for whatever reason, I'm having trouble calling this directly with a large number of arguments
	# -> use eval method instead, since it actually works ...
	stat_cmd=""
	if [ ${STAT_IS_BSD} -gt 0 ]; then
		stat_cmd="stat -f \"%m\" $@"
	else
		stat_cmd="stat -c \"%Y\" $@"
	fi
	echo $(eval $stat_cmd)
}

# figure out which md5 cmd/binary can be used
MD5_CMD=
if [[ $(command -v md5sum) ]]; then
	MD5_CMD=md5sum
elif [[ $(command -v md5) ]]; then
	MD5_CMD=md5
else
	error "neither md5 nor md5sum was found"
fi

# if an explicit job count was specified, overwrite BUILD_CPU_COUNT with it
if [ ${BUILD_JOB_COUNT} -gt 0 ]; then
	BUILD_CPU_COUNT=${BUILD_JOB_COUNT}
fi

# set the target binary name (depends on the platform)
TARGET_BIN_NAME=lib${TARGET_NAME}
PCH_FILE_NAME="floor_prefix.pch"
PCH_BIN_NAME=${TARGET_NAME}.pch
# append 'd' for debug builds
if [ $BUILD_MODE == "debug" ]; then
	TARGET_BIN_NAME=${TARGET_BIN_NAME}d
	PCH_BIN_NAME=${TARGET_NAME}d.pch
fi

# use *.a for all platforms
TARGET_STATIC_BIN_NAME=${TARGET_BIN_NAME}_static.a

# file ending, depending on the platform we're building on
# macOS/iOS -> .dylib
if [ $BUILD_OS == "macos" -o $BUILD_OS == "ios" ]; then
	TARGET_BIN_NAME=${TARGET_BIN_NAME}.dylib
# windows/mingw/cygwin -> .dll
elif [ $BUILD_OS == "mingw" -o $BUILD_OS == "cygwin" ]; then
	TARGET_BIN_NAME=${TARGET_BIN_NAME}.dll
# else -> .so
else
	# default to .so for all other platforms (linux/*bsd/unknown)
	TARGET_BIN_NAME=${TARGET_BIN_NAME}.so
fi

# disable metal support on non-iOS/macOS targets
if [ $BUILD_OS != "macos" -a $BUILD_OS != "ios" ]; then
	BUILD_CONF_METAL=0
fi

# disable VR support on macOS/iOS targets
if [ $BUILD_OS == "macos" -o $BUILD_OS == "ios" ]; then
	BUILD_CONF_OPENVR=0
	BUILD_CONF_OPENXR=0
fi

# try using lld if it is available, otherwise fall back to using clangs default
# NOTE: msys2/mingw lld is not supported
if [ -z "${LD}" ]; then
	if [ $BUILD_OS != "mingw" -a $BUILD_OS != "cygwin" ]; then
		if [[ -n $(command -v ld.lld) ]]; then
			LDFLAGS="${LDFLAGS} -fuse-ld=lld"
		fi
	fi
fi

##########################################
# directory setup
# note that all paths are relative

# binary/library directory where the final binaries will be stored (*.so, *.dylib, etc.)
BIN_DIR=bin

# location of the target binary
TARGET_BIN=${BIN_DIR}/${TARGET_BIN_NAME}
TARGET_STATIC_BIN=${BIN_DIR}/${TARGET_STATIC_BIN_NAME}

# root folder of the source code
SRC_DIR=.

# all source code sub-directories, relative to SRC_DIR
SRC_SUB_DIRS="compute compute/cuda compute/host compute/metal compute/opencl compute/vulkan graphics graphics/metal graphics/vulkan constexpr core floor lang math threading vr"
if [ $BUILD_OS == "macos" -o $BUILD_OS == "ios" ]; then
	SRC_SUB_DIRS="${SRC_SUB_DIRS} darwin"
fi

# build directory where all temporary files are stored (*.o, etc.)
BUILD_DIR=
if [ $BUILD_MODE == "debug" ]; then
	BUILD_DIR=${BUILD_DIR_ROOT}/debug
else
	BUILD_DIR=${BUILD_DIR_ROOT}/release
fi

# current directory + escaped form
CUR_DIR=$(pwd)
ESC_CUR_DIR=$(echo ${CUR_DIR} | sed -E "s/\//\\\\\//g")

# trigger full rebuild if build.sh is newer than the target bin or the target bin doesn't exist
if [ ${BUILD_CLEAN} -eq 0 -a ${BUILD_JSON} -eq 0 ]; then
	if [ ! -f ${BIN_DIR}/${TARGET_BIN_NAME} ]; then
		info "rebuilding because the target bin doesn't exist ..."
		BUILD_REBUILD=1
	elif [ $(file_mod_time build.sh) -gt $(file_mod_time ${BIN_DIR}/${TARGET_BIN_NAME}) ]; then
		info "rebuilding because build.sh is newer than the target bin ..."
		BUILD_REBUILD=1
	fi
fi

##########################################
# library/dependency handling

# initial linker, lib and include setup
LDFLAGS="${LDFLAGS} -fvisibility=default"
if [ ${BUILD_CONF_LIBSTDCXX} -gt 0 ]; then
	LDFLAGS="${LDFLAGS} -stdlib=libstdc++"
else
	LDFLAGS="${LDFLAGS} -stdlib=libc++"
	if [ $BUILD_OS != "macos" -a $BUILD_OS != "ios" ]; then
		# preempt all other libc++ paths
		INCLUDES="${INCLUDES} -isystem /usr/include/c++/v1"
	fi
	INCLUDES="${INCLUDES} -isystem /usr/local/include/c++/v1"
fi
LIBS="${LIBS}"
COMMON_FLAGS="${COMMON_FLAGS}"

# if no AR is specified, set it to the default ar (used when creating a static lib)
if [ -z "${AR}" ]; then
	AR=ar
fi

# set the correct 64-bit linker flag (use the default on mingw)
if [ $BUILD_OS != "mingw" ]; then
	LDFLAGS="${LDFLAGS} -m64"
fi

# handle clang/llvm sanitizers
if [ ${BUILD_CONF_SANITIZERS} -gt 0 ]; then
	if [ ${BUILD_CONF_ASAN} -gt 0 ]; then
		LDFLAGS="${LDFLAGS} -fsanitize=address"
		COMMON_FLAGS="${COMMON_FLAGS} -fsanitize=address -fno-omit-frame-pointer"
	fi
	if [ ${BUILD_CONF_MSAN} -gt 0 ]; then
		LDFLAGS="${LDFLAGS} -fsanitize=memory"
		COMMON_FLAGS="${COMMON_FLAGS} -fsanitize=memory -fno-omit-frame-pointer"
	fi
	if [ ${BUILD_CONF_TSAN} -gt 0 ]; then
		LDFLAGS="${LDFLAGS} -fsanitize=thread"
		COMMON_FLAGS="${COMMON_FLAGS} -fsanitize=thread -fno-omit-frame-pointer"
	fi
	if [ ${BUILD_CONF_UBSAN} -gt 0 ]; then
		LDFLAGS="${LDFLAGS} -fsanitize=undefined-trap -fsanitize-undefined-trap-on-error"
		COMMON_FLAGS="${COMMON_FLAGS} -fsanitize=undefined-trap -fsanitize-undefined-trap-on-error -fno-omit-frame-pointer"
	fi
fi

# use pkg-config (and some manual libs/includes) on all platforms except macOS/iOS
if [ $BUILD_OS != "macos" -a $BUILD_OS != "ios" ]; then
	# build a shared library + need to make kernel symbols visible for dlsym
	LDFLAGS="${LDFLAGS} -shared"
	if [ $BUILD_OS != "mingw" ]; then
		LDFLAGS="${LDFLAGS} -rdynamic"
	fi
	
	# use PIC
	# NOTE: -fno-pic -fno-pie is used at the front to disable/reset any defaults
	LDFLAGS="${LDFLAGS} -fPIC"
	COMMON_FLAGS="${COMMON_FLAGS} -fno-pic -fno-pie -Xclang -mrelocation-model -Xclang pic -Xclang -pic-level -Xclang 2"
	
	# pkg-config: required libraries/packages and optional libraries/packages
	PACKAGES="sdl3"
	PACKAGES_OPT=""
	if [ ${BUILD_CONF_OPENVR} -gt 0 ]; then
		PACKAGES_OPT="${PACKAGES_OPT} openvr"
	fi
	if [ ${BUILD_CONF_OPENXR} -gt 0 ]; then
		PACKAGES_OPT="${PACKAGES_OPT} openxr"
	fi

	# TODO: error checking + check if libs exist
	for pkg in ${PACKAGES}; do
		LIBS="${LIBS} $(pkg-config --libs "${pkg}")"
		COMMON_FLAGS="${COMMON_FLAGS} $(pkg-config --cflags "${pkg}")"
	done
	for pkg in ${PACKAGES_OPT}; do
		LIBS="${LIBS} $(pkg-config --libs "${pkg}")"
		COMMON_FLAGS="${COMMON_FLAGS} $(pkg-config --cflags "${pkg}")"
	done

	# libs that don't have pkg-config
	UNCHECKED_LIBS=""
	if [ $BUILD_OS != "mingw" ]; then
		# must link pthread on unix
		UNCHECKED_LIBS="${UNCHECKED_LIBS} pthread"
	else
		# must link winpthread instead on windows/mingw
		UNCHECKED_LIBS="${UNCHECKED_LIBS} winpthread"
	fi
	if [ ${BUILD_CONF_OPENCL} -gt 0 ]; then
		UNCHECKED_LIBS="${UNCHECKED_LIBS} OpenCL"
	fi

	# add os specific libs
	if [ $BUILD_OS == "linux" -o $BUILD_OS == "freebsd" -o $BUILD_OS == "openbsd" ]; then
		UNCHECKED_LIBS="${UNCHECKED_LIBS} Xxf86vm"
	elif [ $BUILD_OS == "mingw" -o $BUILD_OS == "cygwin" ]; then
		UNCHECKED_LIBS="${UNCHECKED_LIBS} gdi32"
	fi
	
	# linux:
	#  * must also link against c++abi when using libc++
	#  * need to add the /lib folder
	if [ $BUILD_OS == "linux" ]; then
		if [ ${BUILD_CONF_LIBSTDCXX} -eq 0 ]; then
			UNCHECKED_LIBS="${UNCHECKED_LIBS} c++abi"
		fi
		LDFLAGS="${LDFLAGS} -L/lib"
	fi
	
	# windows/mingw opencl and vulkan handling
	if [ $BUILD_OS == "mingw" ]; then
		if [ ${BUILD_CONF_OPENCL} -gt 0 ]; then
			if [ "$(pkg-config --exists OpenCL && echo $?)" == "0" -a "$(pkg-config --exists OpenCL-Headers && echo $?)" == "0" ]; then
				# use MSYS2/MinGW package
				LIBS="${LIBS} $(pkg-config --libs OpenCL) $(pkg-config --libs OpenCL-Headers)"
				COMMON_FLAGS="${COMMON_FLAGS} $(pkg-config --cflags OpenCL) $(pkg-config --cflags OpenCL-Headers)"
			elif [ ! -z "${AMDAPPSDKROOT}" ]; then
				# use amd opencl sdk
				AMDAPPSDKROOT_FIXED=$(echo ${AMDAPPSDKROOT} | sed -E "s/\\\\/\//g")
				LDFLAGS="${LDFLAGS} -L\"${AMDAPPSDKROOT_FIXED}lib/x86_64\""
				INCLUDES="${INCLUDES} -isystem \"${AMDAPPSDKROOT_FIXED}include\""
			elif [ ! -z "${OCL_ROOT}" ]; then
				# use new amd opencl sdk
				OCL_ROOT_FIXED=$(echo ${OCL_ROOT} | sed -E "s/\\\\/\//g")
				LDFLAGS="${LDFLAGS} -L\"${OCL_ROOT_FIXED}/lib/x86_64\""
				INCLUDES="${INCLUDES} -isystem \"${OCL_ROOT_FIXED}/include\""
			elif [ ! -z "${INTELOCLSDKROOT}" ]; then
				# use intel opencl sdk
				INTELOCLSDKROOT_FIXED=$(echo ${INTELOCLSDKROOT} | sed -E "s/\\\\/\//g")
				LDFLAGS="${LDFLAGS} -L\"${INTELOCLSDKROOT_FIXED}lib/x64\""
				INCLUDES="${INCLUDES} -isystem \"${INTELOCLSDKROOT_FIXED}include\""
			else
				error "building with OpenCL support, but no OpenCL SDK was found - please install the Intel or AMD OpenCL SDK!"
			fi
		fi
		
		if [ ${BUILD_CONF_VULKAN} -gt 0 ]; then
			if [ "$(pkg-config --exists vulkan && echo $?)" == "0" ]; then
				# -> use MSYS2/MinGW package includes
				:
			elif [ ! -z "${VK_SDK_PATH}" ]; then
				# -> use official SDK includes
				INCLUDES="${INCLUDES} -isystem \"${VK_SDK_PATH_FIXED}/Include\""
			else
				error "Vulkan SDK not installed (install official SDK or mingw-w64-x86_64-vulkan)"
			fi
		fi
	fi

	for lib in ${UNCHECKED_LIBS}; do
		LIBS="${LIBS} -l${lib}"
	done
	
	# mingw: "--allow-multiple-definition" is necessary, because gcc is still used as a linker
	# and will always link against libstdc++/libsupc++ (-> multiple definitions with libc++)
	# also note: since libc++ is linked first, libc++'s functions will be used
	if [ $BUILD_OS == "mingw" -a ${BUILD_CONF_LIBSTDCXX} -eq 0 ]; then
		LDFLAGS="${LDFLAGS} -lc++.dll -Wl,--allow-multiple-definition -lsupc++"
	fi
	
	# needed for ___chkstk_ms
	if [ $BUILD_OS == "mingw" ]; then
		LDFLAGS="${LDFLAGS} -lgcc"
	fi
	
	# add all libs to LDFLAGS
	LDFLAGS="${LDFLAGS} ${LIBS}"
else
	# on macOS/iOS: assume everything is installed, pkg-config doesn't really exist
	if [ ${BUILD_CONF_OPENVR} -gt 0 ]; then
		INCLUDES="${INCLUDES} -isystem /usr/local/include/openvr"
	fi
	if [ ${BUILD_CONF_OPENXR} -gt 0 ]; then
		INCLUDES="${INCLUDES} -isystem /usr/local/include/openxr"
	fi
	INCLUDES="${INCLUDES} -iframework /Library/Frameworks"
	
	# build a shared/dynamic library
	LDFLAGS="${LDFLAGS} -dynamiclib"
	
	# additional lib/framework paths
	LDFLAGS="${LDFLAGS} -F/Library/Frameworks -L/usr/local/lib"

	# rpath voodoo
	LDFLAGS="${LDFLAGS} -install_name @rpath/${TARGET_BIN_NAME}"
	LDFLAGS="${LDFLAGS} -Xlinker -rpath -Xlinker @loader_path/../Resources"
	LDFLAGS="${LDFLAGS} -Xlinker -rpath -Xlinker @loader_path/../Frameworks"
	LDFLAGS="${LDFLAGS} -Xlinker -rpath -Xlinker /Library/Frameworks"
	LDFLAGS="${LDFLAGS} -Xlinker -rpath -Xlinker /usr/local/lib"
	LDFLAGS="${LDFLAGS} -Xlinker -rpath -Xlinker /usr/lib"
	LDFLAGS="${LDFLAGS} -Xlinker -rpath -Xlinker /opt/floor/lib"
	
	# probably necessary
	LDFLAGS="${LDFLAGS} -fobjc-link-runtime"
	
	# frameworks and libs
	LDFLAGS="${LDFLAGS} -framework SDL3"
	if [ ${BUILD_CONF_OPENVR} -gt 0 ]; then
		LDFLAGS="${LDFLAGS} -lopenvr_api"
	fi
	if [ ${BUILD_CONF_OPENXR} -gt 0 ]; then
		LDFLAGS="${LDFLAGS} -lopenxr_loader"
	fi
	
	# system frameworks
	LDFLAGS="${LDFLAGS} -framework ApplicationServices -framework AppKit -framework Cocoa -framework QuartzCore"
	if [ ${BUILD_CONF_METAL} -gt 0 ]; then
		LDFLAGS="${LDFLAGS} -framework Metal"
	fi
fi

# just in case, also add these rather default ones (should also go after all previous libs,
# in case a local or otherwise set up lib is overwriting a system lib and should be used instead)
LDFLAGS="${LDFLAGS} -L/usr/lib -L/usr/local/lib"

# create the floor_conf.hpp file
if [ ${BUILD_CLEAN} -eq 0 -a ${BUILD_JSON} -eq 0 ]; then
	CONF=$(cat floor/floor_conf.hpp.in)
	set_conf_val() {
		repl_text="$1"
		define="$2"
		enabled=$3
		if [ ${enabled} -gt 0 ]; then
			CONF=$(echo "${CONF}" | sed -E "s/${repl_text}/\/\/#define ${define} 1/g")
		else
			CONF=$(echo "${CONF}" | sed -E "s/${repl_text}/#define ${define} 1/g")
		fi
	}
	set_conf_val "###FLOOR_CUDA###" "FLOOR_NO_CUDA" ${BUILD_CONF_CUDA}
	set_conf_val "###FLOOR_OPENCL###" "FLOOR_NO_OPENCL" ${BUILD_CONF_OPENCL}
	set_conf_val "###FLOOR_HOST_COMPUTE###" "FLOOR_NO_HOST_COMPUTE" ${BUILD_CONF_HOST_COMPUTE}
	set_conf_val "###FLOOR_VULKAN###" "FLOOR_NO_VULKAN" ${BUILD_CONF_VULKAN}
	# NOTE: metal is disabled on non-ios platforms anyways and this would overwrite the ios flag that is already ifdef'ed
	if [ $BUILD_OS != "ios" ]; then
		set_conf_val "###FLOOR_METAL###" "FLOOR_NO_METAL" 1
	else
		# -> only actually set the config option if we're building on ios
		set_conf_val "###FLOOR_METAL###" "FLOOR_NO_METAL" ${BUILD_CONF_METAL}
	fi
	set_conf_val "###FLOOR_OPENVR###" "FLOOR_NO_OPENVR" ${BUILD_CONF_OPENVR}
	set_conf_val "###FLOOR_OPENXR###" "FLOOR_NO_OPENXR" ${BUILD_CONF_OPENXR}
	echo "${CONF}" > floor/floor_conf.hpp.tmp

	# check if this is an entirely new conf or if it differs from the existing conf
	if [ ! -f floor/floor_conf.hpp ]; then
		mv floor/floor_conf.hpp.tmp floor/floor_conf.hpp
	elif [ "$(${MD5_CMD} floor/floor_conf.hpp.tmp | cut -c -32)" != "$(${MD5_CMD} floor/floor_conf.hpp | cut -c -32)" ]; then
		info "new conf file ..."
		mv -f floor/floor_conf.hpp.tmp floor/floor_conf.hpp
	fi

	# only update build version if FLOOR_DEV environment variable is set
	if [ -n "${FLOOR_DEV}" ]; then
		# checks if any source files have updated (are newer than the target binary)
		# if so, this increments the build version by one (updates the header file)
		info "build version update ..."
		. ./floor/build_version.sh
	fi
fi

# version of the target (preprocess the floor version header, grep the version defines, transform them to exports and eval)
eval $(${CXX} -E -dM -I../ floor/floor_version.hpp 2>&1 | grep -E "define (FLOOR_(MAJOR|MINOR|REVISION|DEV_STAGE|BUILD)_VERSION|FLOOR_DEV_STAGE_VERSION_STR) " | sed -E "s/.*define (.*) [\"]*([^ \"]*)[\"]*/export \1=\2/g")
TARGET_VERSION="${FLOOR_MAJOR_VERSION}.${FLOOR_MINOR_VERSION}.${FLOOR_REVISION_VERSION}"
TARGET_FULL_VERSION="${TARGET_VERSION}${FLOOR_DEV_STAGE_VERSION_STR}-${FLOOR_BUILD_VERSION}"
if [ ${BUILD_CLEAN} -eq 0 -a ${BUILD_JSON} -eq 0 ]; then
	info "building ${TARGET_NAME} v${TARGET_FULL_VERSION} (${BUILD_MODE})"
fi

##########################################
# flags

# set up initial c++ and c flags
CXXFLAGS="${CXXFLAGS} -std=gnu++2b"
if [ ${BUILD_CONF_LIBSTDCXX} -gt 0 ]; then
	CXXFLAGS="${CXXFLAGS} -stdlib=libstdc++"
else
	CXXFLAGS="${CXXFLAGS} -stdlib=libc++"
fi
CFLAGS="${CFLAGS} -std=gnu17"

OBJCFLAGS="${OBJCFLAGS} -fno-objc-exceptions"
if [ $BUILD_OS == "macos" -o $BUILD_OS == "ios" ]; then
	OBJCFLAGS="${OBJCFLAGS} -fobjc-arc"
fi

# so not standard compliant ...
if [ $BUILD_OS == "mingw" ]; then
	CXXFLAGS="${CXXFLAGS} -pthread"
fi

# arch handling (use -arch on macOS/iOS and -m64 everywhere else, except for mingw)
if [ $BUILD_OS == "macos" -o $BUILD_OS == "ios" ]; then
	case $BUILD_ARCH in
		"arm"*)
			COMMON_FLAGS="${COMMON_FLAGS} -arch arm64"
			;;
		*)
			COMMON_FLAGS="${COMMON_FLAGS} -arch x86_64"
			;;
	esac
elif [ $BUILD_OS != "mingw" ]; then
	# NOTE: mingw will/should/has to use the compiler default
	COMMON_FLAGS="${COMMON_FLAGS} -m64"
fi

# c++ and c flags that apply to all build configurations
COMMON_FLAGS="${COMMON_FLAGS} -ffast-math -fstrict-aliasing"

# set flags when building for the native/host cpu
if [ $BUILD_CONF_NATIVE -gt 0 ]; then
	COMMON_FLAGS="${COMMON_FLAGS} -march=native -mtune=native"
fi

# debug flags, only used in the debug target
DEBUG_FLAGS="-O0 -DFLOOR_DEBUG=1 -DDEBUG -fno-limit-debug-info"
if [ $BUILD_OS != "mingw" ]; then
	DEBUG_FLAGS="${DEBUG_FLAGS} -gdwarf-4"
else
	DEBUG_FLAGS="${DEBUG_FLAGS} -g"
fi

# release mode flags/optimizations
REL_FLAGS="-Ofast -funroll-loops"
# if we're building for the native/host cpu, the appropriate sse/avx flags will already be set/used
if [ $BUILD_CONF_NATIVE -eq 0 ]; then
	if [ $BUILD_OS != "macos" -a $BUILD_OS != "ios" ]; then
		# TODO: sse/avx selection/config? default to sse4.1 for now (core2)
		REL_FLAGS="${REL_FLAGS} -msse4.1"
	fi
fi

# additional optimizations (used in addition to REL_CXX_FLAGS)
REL_OPT_FLAGS="-flto"
REL_OPT_LD_FLAGS="-flto"

# macOS/iOS: set min version
if [ $BUILD_OS == "macos" -o $BUILD_OS == "ios" ]; then
	if [ $BUILD_OS == "macos" ]; then
		COMMON_FLAGS="${COMMON_FLAGS} -mmacos-version-min=13.0"
	else # ios
		COMMON_FLAGS="${COMMON_FLAGS} -mios-version-min=16.0"
	fi
	
	# set lib version
	LDFLAGS="${LDFLAGS} -compatibility_version ${TARGET_VERSION} -current_version ${TARGET_VERSION}"
fi

# defines:
if [ $BUILD_OS == "mingw" -o $BUILD_OS == "cygwin" ]; then
	# common windows "unix environment" flag
	COMMON_FLAGS="${COMMON_FLAGS} -DWIN_UNIXENV"
	if [ $BUILD_OS == "mingw" ]; then
		# set __WINDOWS__ and mingw specific flag
		COMMON_FLAGS="${COMMON_FLAGS} -D__WINDOWS__ -DMINGW"
	fi
	if [ $BUILD_OS == "cygwin" ]; then
		# set cygwin specific flag
		COMMON_FLAGS="${COMMON_FLAGS} -DCYGWIN"
	fi
fi
# always use extern templates
COMMON_FLAGS="${COMMON_FLAGS} -DFLOOR_EXPORT=1"

# hard-mode c++ ;)
# let's start with everything
WARNINGS="-Weverything ${WARNINGS}"
# in case we're using warning options that aren't supported by other clang versions
WARNINGS="${WARNINGS} -Wno-unknown-warning-option"
# remove std compat warnings (C++23 with gnu and clang extensions is required)
WARNINGS="${WARNINGS} -Wno-c++98-compat -Wno-c++98-compat-pedantic"
WARNINGS="${WARNINGS} -Wno-c++11-compat -Wno-c++11-compat-pedantic"
WARNINGS="${WARNINGS} -Wno-c++14-compat -Wno-c++14-compat-pedantic"
WARNINGS="${WARNINGS} -Wno-c++17-compat -Wno-c++17-compat-pedantic"
WARNINGS="${WARNINGS} -Wno-c++20-compat -Wno-c++20-compat-pedantic -Wno-c++20-extensions"
WARNINGS="${WARNINGS} -Wno-c++23-compat -Wno-c++23-compat-pedantic -Wno-c++23-extensions"
WARNINGS="${WARNINGS} -Wno-c99-extensions -Wno-c11-extensions"
WARNINGS="${WARNINGS} -Wno-gnu -Wno-gcc-compat"
WARNINGS="${WARNINGS} -Wno-nullability-extension"
# don't be too pedantic
WARNINGS="${WARNINGS} -Wno-header-hygiene -Wno-documentation -Wno-documentation-unknown-command -Wno-old-style-cast"
WARNINGS="${WARNINGS} -Wno-global-constructors -Wno-exit-time-destructors -Wno-reserved-id-macro -Wno-reserved-identifier"
WARNINGS="${WARNINGS} -Wno-date-time -Wno-poison-system-directories"
# suppress warnings in system headers
WARNINGS="${WARNINGS} -Wno-system-headers"
# these two are only useful in certain situations, but are quite noisy
WARNINGS="${WARNINGS} -Wno-packed -Wno-padded"
# these conflict with the other switch/case warning
WARNINGS="${WARNINGS} -Wno-switch-enum -Wno-switch-default"
# quite useful feature/extension
WARNINGS="${WARNINGS} -Wno-nested-anon-types"
# this should be taken care of in a different way
WARNINGS="${WARNINGS} -Wno-partial-availability"
# enable thread-safety warnings
WARNINGS="${WARNINGS} -Wthread-safety -Wthread-safety-negative -Wthread-safety-beta -Wthread-safety-verbose"
# ignore "explicit move to avoid copying on older compilers" warning
WARNINGS="${WARNINGS} -Wno-return-std-move-in-c++11"
# ignore unsafe pointer/buffer access warnings
WARNINGS="${WARNINGS} -Wno-unsafe-buffer-usage"
# ignore reserved identifier warnings because of "__" prefixes
WARNINGS="${WARNINGS} -Wno-reserved-identifier"
# ignore UD NaN/infinity due to fast-math
WARNINGS="${WARNINGS} -Wno-nan-infinity-disabled"
# ignore warnings about missing designated initializer when they are default-initialized
# on clang < 19: disable missing field initializer warnings altogether
if [ $CLANG_MAJOR -ge 19 ]; then
	WARNINGS="${WARNINGS} -Wno-missing-designated-field-initializers"
else
	WARNINGS="${WARNINGS} -Wno-missing-field-initializers"
fi
COMMON_FLAGS="${COMMON_FLAGS} ${WARNINGS}"

# diagnostics
COMMON_FLAGS="${COMMON_FLAGS} -fdiagnostics-show-note-include-stack -fmessage-length=0 -fmacro-backtrace-limit=0"
COMMON_FLAGS="${COMMON_FLAGS} -fparse-all-comments -fno-elide-type -fdiagnostics-show-template-tree"

# includes + replace all "-I"s with "-isystem"s so that we don't get warnings in external headers
COMMON_FLAGS="${COMMON_FLAGS} ${INCLUDES}"
COMMON_FLAGS=$(echo "${COMMON_FLAGS}" | sed -E "s/-I/-isystem /g")

# include libfloor itself (so that we can use <floor/...> includes)
if [ -z "${FLOOR_SELF_INCL_DIR}" ]; then
	# default: ../ -> finds ../floor
	COMMON_FLAGS="${COMMON_FLAGS} -I../"
else
	COMMON_FLAGS="${COMMON_FLAGS} -I${FLOOR_SELF_INCL_DIR}"
fi

# mingw fixes/workarounds
if [ $BUILD_OS == "mingw" ]; then
	# remove sdls main redirect, we want to use our own main
	COMMON_FLAGS=$(echo "${COMMON_FLAGS}" | sed -E "s/-Dmain=SDL_main//g")
	# don't include "mingw64/include" directly
	COMMON_FLAGS=$(echo "${COMMON_FLAGS}" | sed -E "s/-isystem ([A-Za-z0-9_\-\:\/\.\(\) ]+)mingw64\/include //g")
	# remove windows flag -> creates a separate cmd window + working iostream output
	LDFLAGS=$(echo "${LDFLAGS}" | sed -E "s/-mwindows //g")
	# remove unwanted -static-libgcc flag (this won't work and lead to linker errors!)
	LDFLAGS=$(echo "${LDFLAGS}" | sed -E "s/-static-libgcc //g")
	# remove unwanted -lm (this won't work and lead to linker errors!)
	LDFLAGS=$(echo "${LDFLAGS}" | sed -E "s/-lm //g")
	# remove unwanted -ldl (this doesn't exist on Windows)
	LDFLAGS=$(echo "${LDFLAGS}" | sed -E "s/-ldl //g")
fi

# mingw: create import lib and export everything
if [ $BUILD_OS == "mingw" ]; then
	LDFLAGS="${LDFLAGS} -Wl,--export-all-symbols -Wl,-no-undefined -Wl,--enable-runtime-pseudo-reloc -Wl,--out-implib,${BIN_DIR}/${TARGET_BIN_NAME}.a"
fi

# misc workarounds
# remove direct and indirect /usr/include paths
COMMON_FLAGS=$(echo "${COMMON_FLAGS}" | sed -E "s/-isystem \/usr\/lib\/include //g")
COMMON_FLAGS=$(echo "${COMMON_FLAGS}" | sed -E "s/-isystem \/usr\/lib\/pkgconfig\/..\/..\/include //g")

# finally: add all common c++ and c flags/options
CXXFLAGS="${CXXFLAGS} ${COMMON_FLAGS}"
CFLAGS="${CFLAGS} ${COMMON_FLAGS}"

##########################################
# targets and building

# get all source files (c++/c/objective-c++/objective-c) and create build folders
for dir in ${SRC_SUB_DIRS}; do
	# source files
	SRC_FILES="${SRC_FILES} $(find ${dir} -maxdepth 1 -type f -name '*.cpp' | grep -v "\._")"
	SRC_FILES="${SRC_FILES} $(find ${dir} -maxdepth 1 -type f -name '*.c' | grep -v "\._")"
	SRC_FILES="${SRC_FILES} $(find ${dir} -maxdepth 1 -type f -name '*.mm' | grep -v "\._")"
	SRC_FILES="${SRC_FILES} $(find ${dir} -maxdepth 1 -type f -name '*.m' | grep -v "\._")"
	
	# create resp. build folder
	mkdir -p ${BUILD_DIR}/${dir}
done

# total hack, but it makes building faster: reverse the source file order, so that math files
# are compiled first (these take longest to compile, so pipeline them properly)
# also: put compute/compute_image.cpp at the front, because it takes the longest to compile
REV_SRC_FILES=""
for source_file in ${SRC_FILES}; do
	if [ $source_file != "compute/compute_image.cpp" ]; then
		REV_SRC_FILES="${source_file} ${REV_SRC_FILES}"
	fi
done
SRC_FILES="compute/compute_image.cpp ${REV_SRC_FILES}"

# make a list of all object files
for source_file in ${SRC_FILES}; do
	OBJ_FILES="${OBJ_FILES} ${BUILD_DIR}/${source_file}.o"
done

# set flags depending on the build mode, or make a clean exit
if [ ${BUILD_CLEAN} -gt 0 ]; then
	# delete the target binary and the complete build folder (all object files)
	info "cleaning ${BUILD_MODE} ..."
	rm -f ${TARGET_BIN}
	rm -f ${TARGET_STATIC_BIN}
	rm -f ${PCH_BIN_NAME}
	rm -Rf ${BUILD_DIR}
	exit 0
else
	case ${BUILD_MODE} in
		"release")
			# release mode (default): add release mode flags/optimizations
			CXXFLAGS="${CXXFLAGS} ${REL_FLAGS}"
			CFLAGS="${CFLAGS} ${REL_FLAGS}"
			;;
		"release_opt")
			# release mode + additional optimizations: add release mode and opt flags
			CXXFLAGS="${CXXFLAGS} ${REL_FLAGS} ${REL_OPT_FLAGS}"
			CFLAGS="${CFLAGS} ${REL_FLAGS} ${REL_OPT_FLAGS}"
			LDFLAGS="${LDFLAGS} ${REL_OPT_LD_FLAGS}"
			;;
		"debug")
			# debug mode: add debug flags
			CXXFLAGS="${CXXFLAGS} ${DEBUG_FLAGS}"
			CFLAGS="${CFLAGS} ${DEBUG_FLAGS}"
			;;
		*)
			error "unknown build mode: ${BUILD_MODE}"
			;;
	esac
fi

# build the compile_commands.json file if specified
if [ ${BUILD_JSON} -gt 0 ]; then
	file_count=$(echo "${SRC_FILES}" | wc -w | tr -d [:space:])
	file_counter=0
	
	cmds="[\n"
	for source_file in ${SRC_FILES}; do
		file_counter=$(expr $file_counter + 1)
		case ${source_file} in
			*".cpp")
				build_cmd="${CXX} -include-pch ${PCH_BIN_NAME} ${CXXFLAGS}"
				;;
			*".c")
				build_cmd="${CC} ${CFLAGS}"
				;;
			*".mm")
				build_cmd="${CXX} -x objective-c++ ${OBJCFLAGS} ${CXXFLAGS}"
				;;
			*".m")
				build_cmd="${CC} -x objective-c ${OBJCFLAGS} ${CFLAGS}"
				;;
			*)
				error "unknown source file ending: ${source_file}"
				;;
		esac
		# NOTE: we're not adding the header dependency list creation flags here
		build_cmd="${build_cmd} -c ${FLOOR_SRC_PREFIX_DIR}${source_file} -o ${BUILD_DIR}/${source_file}.o"
		cmds="${cmds}\t{\n\t\t\"directory\": \"${CUR_DIR}\",\n\t\t\"command\": \"${build_cmd}\",\n\t\t\"file\": \"${FLOOR_SRC_PREFIX_DIR}${source_file}\"\n\t}"
		if [ $file_counter -lt $file_count ]; then
			cmds="${cmds},\n"
		else
			cmds="${cmds}\n"
		fi
	done
	cmds="${cmds}]\n"
	
	printf "${cmds}" > compile_commands.json
	
	exit 0
fi

#
if [ ${BUILD_VERBOSE} -gt 1 ]; then
	CXXFLAGS="${CXXFLAGS} -v"
	CFLAGS="${CFLAGS} -v"
	LDFLAGS="${LDFLAGS} -v"
fi
if [ ${BUILD_VERBOSE} -gt 0 ]; then
	info ""
	info "using CXXFLAGS: ${CXXFLAGS}"
	info ""
	info "using CFLAGS: ${CFLAGS}"
	info ""
	info "using LDFLAGS: ${LDFLAGS}"
	info ""
fi

# this function checks if a source file needs rebuilding, based on its dependency list that has been generated in a previos build
# -> this will check the modification date of the source files object/build file against all dependency files
# -> if any is newer, this source file needs to be rebuild
# -> will also rebuild if either the object/build file or dependency file don't exist, or the "rebuild" build mode is set
needs_rebuild() {
	source_file=$1

	bin_file_name="${BUILD_DIR}/${source_file}.o"
	if [ ${source_file} == ${PCH_FILE_NAME} ]; then
		bin_file_name="${PCH_BIN_NAME}"
	fi

	rebuild_file=0
	if [ ! -f ${BUILD_DIR}/${source_file}.d -o ! -f ${bin_file_name} -o ${BUILD_REBUILD} -gt 0 ]; then
		info "rebuild because >${BUILD_DIR}/${source_file}.d< doesn't exist or BUILD_REBUILD $BUILD_REBUILD"
		rebuild_file=1
	else
		dep_list=$(cat ${BUILD_DIR}/${source_file}.d | sed -E "s/deps://" | sed -E "s/ \\\\//" | sed -E "s/${ESC_CUR_DIR}\/\.\.\/floor\//${ESC_CUR_DIR}\//g" | sed -E "s/\.\.\/floor\//${ESC_CUR_DIR}\//g" | sed -E "s/\.\.\\\\floor\//${ESC_CUR_DIR}\//g" | tr ' ' '\n' | grep -v -E '^$' | sort | uniq | sed -E "s/([^[:space:]]*)$/\"\0\"/g")
		if [ "${dep_list}" ]; then
			file_time=$(file_mod_time "${bin_file_name}")
			dep_times=$(file_mod_time ${dep_list})
			for dep_time in ${dep_times}; do
				if [ $dep_time -gt $file_time ]; then
					rebuild_file=1
					break
				fi
			done
		fi
	fi
	if [ $rebuild_file -gt 0 ]; then
		echo "1"
	fi
}

# build the precompiled header
rebuild_pch=0
rebuild_pch_info=$(needs_rebuild ${PCH_FILE_NAME})
if [ ! -f "${PCH_BIN_NAME}" ]; then
	info "building precompiled header ..."
	rebuild_pch=1
elif [ "${rebuild_pch_info}" ]; then
	info "rebuilding precompiled header ..."
	# -> kill old pch file if it exists
	if [ -f "${PCH_BIN_NAME}" ]; then
		rm ${PCH_BIN_NAME}
	fi
	rebuild_pch=1
fi

if [ $rebuild_pch -gt 0 ]; then
	precomp_header_cmd="${CXX} ${CXXFLAGS} -x c++-header ${PCH_FILE_NAME} -Xclang -emit-pch -o ${PCH_BIN_NAME} -MD -MT deps -MF ${BUILD_DIR}/${PCH_FILE_NAME}.d"
	verbose "${precomp_header_cmd}"
	eval ${precomp_header_cmd}

	# also signal that we need to rebuild all source code
	BUILD_REBUILD=1
fi

if [ ! -f "${PCH_BIN_NAME}" ]; then
	error "precompiled header compilation failed"
	exit -1
fi

# build the target
export build_error=false
trap build_error=true USR1
build_file() {
	# this function builds one source file
	source_file=$1
	file_num=$2
	file_count=$3
	parent_pid=$4

	rebuild_file=$(needs_rebuild ${source_file})
	if [ "${rebuild_file}" ]; then
		info "building ${FLOOR_SRC_PREFIX_DIR}${source_file} [${file_num}/${file_count}]"
		case ${source_file} in
			*".cpp")
				build_cmd="${CXX} -include-pch ${PCH_BIN_NAME} ${CXXFLAGS}"
				;;
			*".c")
				build_cmd="${CC} ${CFLAGS}"
				;;
			*".mm")
				build_cmd="${CXX} -x objective-c++ ${OBJCFLAGS} ${CXXFLAGS}"
				;;
			*".m")
				build_cmd="${CC} -x objective-c ${OBJCFLAGS} ${CFLAGS}"
				;;
			*)
				error "unknown source file ending: ${source_file}"
				;;
		esac
		build_cmd="${build_cmd} -c ${FLOOR_SRC_PREFIX_DIR}${source_file} -o ${BUILD_DIR}/${source_file}.o -MD -MT deps -MF ${BUILD_DIR}/${source_file}.d"
		verbose "${build_cmd}"
		eval ${build_cmd}

		# handle errors
		ret_code=$?
		if [ ${ret_code} -ne 0 ]; then
			kill -USR1 ${parent_pid}
			error "compilation failed (${FLOOR_SRC_PREFIX_DIR}${source_file})"
		fi
	fi
}
job_count() {
	echo $(jobs -p | wc -l)
}
handle_build_errors() {
	# abort on build errors
	if [ "${build_error}" == "true" ]; then
		# wait until all build jobs have finished (all error output has been written)
		wait
		exit -1
	fi
}

# get the amount of source files and create a counter (this is used for some info/debug output)
file_count=$(echo "${SRC_FILES}" | wc -w | tr -d [:space:])
file_counter=0
# iterate over all source files and create a build job for each of them
for source_file in ${SRC_FILES}; do
	file_counter=$(expr $file_counter + 1)
	
	# if only one build job should be used, don't bother with shell jobs
	# this also works around an issue where "jobs -p" always lists one job even after it has finished
	if [ $BUILD_CPU_COUNT -gt 1 ]; then
		# make sure that there are only $BUILD_CPU_COUNT active jobs at any time,
		# this should be the most efficient setup for concurrently building multiple files
		while true; do
			cur_job_count=$(job_count)
			if [ $cur_job_count -lt $BUILD_CPU_COUNT ]; then
				break
			fi
			sleep 0.1
		done
		(build_file $source_file $file_counter $file_count $$) &
	else
		build_file $source_file $file_counter $file_count $$
	fi

	# early build error test
	handle_build_errors
done
# all jobs were started, now we just have to wait until all are done
sleep 0.1
info "waiting for build jobs to finish ..."
wait

# check for build errors again after everything has completed
handle_build_errors

# link
mkdir -p ${BIN_DIR}

relink_target=${BUILD_REBUILD}
relink_static_target=${BUILD_REBUILD}
relink_any=${BUILD_REBUILD}
if [ ! -f ${TARGET_BIN} ]; then
	relink_target=1
	relink_any=1
fi
if [ $BUILD_STATIC -gt 0 ]; then
	if [ -f ${TARGET_STATIC_BIN} ]; then
		relink_static_target=1
		relink_any=1
	fi
fi

if [ $relink_any -eq 0 -a ${BUILD_REBUILD} -eq 0 ]; then
	target_time=$(file_mod_time "${TARGET_BIN}")
	static_target_time=0
	if [ $BUILD_STATIC -gt 0 ]; then
		static_target_time=$(file_mod_time "${TARGET_STATIC_BIN}")
	fi
	obj_times=$(file_mod_time "${OBJ_FILES}")
	for obj_time in ${obj_times}; do
		if [ $obj_time -gt $target_time ]; then
			relink_target=1
		fi
		if [ $BUILD_STATIC -gt 0 ]; then
			if [ $obj_time -gt $static_target_time ]; then
				relink_static_target=1
			fi
		fi
	done
fi

if [ $relink_target -gt 0  ]; then
	info "linking ..."
	linker_cmd="${CXX} -o ${BUILD_DIR}/${TARGET_BIN_NAME} ${OBJ_FILES} ${LDFLAGS}"
	verbose "${linker_cmd}"
	eval ${linker_cmd}
	mv ${BUILD_DIR}/${TARGET_BIN_NAME} ${TARGET_BIN}
fi

if [ $relink_static_target -gt 0 -a $BUILD_STATIC -gt 0 ]; then
	info "linking static ..."
	verbose "${AR} rs ${TARGET_STATIC_BIN} ${OBJ_FILES}"
	${AR} rs ${TARGET_STATIC_BIN} ${OBJ_FILES}
fi

info "built ${TARGET_NAME} v${TARGET_FULL_VERSION}"
