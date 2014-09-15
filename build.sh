#!/bin/sh

# TODO: intro

##########################################
# helper functions
error() {
	printf "\033[1;31m>> $@ \033[m\n"
	exit -1
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
# arg handling
BUILD_MODE="release"
BUILD_VERBOSE=0

# TODO: install/uninstall?
# TODO: static lib
for arg in "$@"; do
	case $arg in
		"--help")
			info "build help"
			echo ""
			echo "usage:"
			echo "	<default>	builds this project in release mode"
			echo "	--opt		builds this project in release mode + additional optimizations that take longer to compile (lto)"
			echo "	--debug		builds this project in debug mode"
			echo "	--clean		cleans all build binaries and intermediate build files"
			echo "	-v		verbose output (prints all executed compiler and linker commands, and some other information)"
			echo "	-vv		very verbose output (same as -v + runs all compiler and linker commands with -v)"
			echo ""
			exit 0
			;;
		"--opt")
			BUILD_MODE="release_opt"
			;;
		"--debug")
			BUILD_MODE="debug"
			;;
		"--clean")
			BUILD_MODE="clean"
			;;
		"-v")
			BUILD_VERBOSE=1
			;;
		"-vv")
			BUILD_VERBOSE=2
			;;
# TODO
#		"--cuda")
#			BUILD_ARGS=${BUILD_ARGS}" --cuda"
#			;;
#		"--pocl")
#			BUILD_ARGS=${BUILD_ARGS}" --pocl"
#			;;
#		"--cl-profiling")
#			BUILD_ARGS=${BUILD_ARGS}" --cl-profiling"
#			;;
		*)
			;;
	esac
done

##########################################
# target and build environment setup

# name of the target (part of the binary name)
TARGET_NAME=floor

# check on which platform we're compiling
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
		;;
	"linux")
		BUILD_OS="linux"
		# note that this includes hyper-threading and multi-socket systems
		BUILD_CPU_COUNT=$(cat /proc/cpuinfo | grep "processor" | wc -l)
		;;
	"freebsd")
		BUILD_OS="freebsd"
		BUILD_CPU_COUNT=$(sysctl -n hw.ncpu)
		;;
	"openbsd")
		BUILD_OS="openbsd"
		BUILD_CPU_COUNT=$(sysctl -n hw.ncpu)
		;;
	"cygwin"*)
		# untested
		BUILD_OS="cygwin"
		BUILD_CPU_COUNT=$(env | grep 'NUMBER_OF_PROCESSORS' | sed -E 's/.*=([:digit:]*)/\1/g')
		warning "cygwin support is untested!"
		;;
	"mingw"*)
		BUILD_OS="mingw"
		BUILD_CPU_COUNT=$(env | grep 'NUMBER_OF_PROCESSORS' | sed -E 's/.*=([:digit:]*)/\1/g')
		;;
	*)
		warning "unknown build platform - trying to continue! ${BUILD_PLATFORM}"
		;;
esac

# set the target binary name (depends on the platform)
TARGET_BIN_NAME=lib${TARGET_NAME}
# append 'd' for debug builds
if [ $BUILD_MODE == "debug" ]; then
	TARGET_BIN_NAME=${TARGET_BIN_NAME}d
fi
# osx/ios -> .dylib
if [ $BUILD_OS == "osx" -o $BUILD_OS == "ios" ]; then
	TARGET_BIN_NAME=${TARGET_BIN_NAME}.dylib
# windows/mingw/cygwin -> .dll
elif [ $BUILD_OS == "mingw" -o $BUILD_OS == "cygwin" ]; then
	TARGET_BIN_NAME=${TARGET_BIN_NAME}.dll
# else -> .so
else
	# default to .so for all other platforms (linux/*bsd/unknown)
	TARGET_BIN_NAME=${TARGET_BIN_NAME}.so
fi

##########################################
# directory setup
# note that all paths are relative

# binary/library directory where the final binaries will be stored (*.so, *.dylib, etc.)
BIN_DIR=bin

# location of the target binary
TARGET_BIN=${BIN_DIR}/${TARGET_BIN_NAME}

# root folder of the source code
SRC_DIR=.

# all source code sub-directories, relative to SRC_DIR
SRC_SUB_DIRS=(audio cl constexpr core cuda floor hash lang math net net/boost_system tccpp threading)
if [ $BUILD_OS == "osx" ]; then
	SRC_SUB_DIRS=(${SRC_SUB_DIRS[@]} osx)
elif [ $BUILD_OS == "ios" ]; then
	SRC_SUB_DIRS=(${SRC_SUB_DIRS[@]} ios)
fi

# build directory where all temporary files are stored (*.o, etc.)
BUILD_DIR=build

##########################################
# compiler setup

# if no CXX/CC are specified, try using clang++/clang
if [ -z "${CXX}" ]; then
	CXX=clang++
fi
if [ -z "${CC}" ]; then
	CC=clang
fi

# check if clang is the compiler, fail if not
CXX_VERSION=$(${CXX} -v 2>&1)
if expr "${CXX_VERSION}" : ".*clang" >/dev/null; then
	# also check the clang version
	if expr "${CXX_VERSION}" : "Apple.*" >/dev/null; then
		# apple xcode/llvm/clang versioning scheme -> at least 6.1 is required
		if expr "$(echo ${CXX_VERSION} | head -n 1 | sed -E "s/Apple LLVM version ([0-9.]+) .*/\1/")" \< "6.1" >/dev/null; then
			error "at least Xcode/LLVM/clang 6.1 is required to compile this project!"
		fi
	else
		# standard clang versioning scheme -> at least 3.5.0 is required
		if expr "$(echo ${CXX_VERSION} | head -n 1 | sed -E "s/.*clang version ([0-9.]+) .*/\1/")" \< "3.5.0" >/dev/null; then
			error "at least clang 3.5.0 is required to compile this project!"
		fi
	fi
else
	error "only clang is currently supported - please set CXX/CC to clang++/clang and try again!"
fi

##########################################
# library/dependency handling

# initial lib and include setup
LDFLAGS="${LDFLAGS}"
LIBS="${LIBS}"
INCLUDES="${INCLUDES}"
COMMON_FLAGS="${COMMON_FLAGS}"

# use pkg-config (and some manual libs/includes) on all platforms except osx/ios
if [ $BUILD_OS != "osx" -a $BUILD_OS != "ios" ]; then
	# pkg-config: required libraries/packages and optional libraries/packages
	PACKAGES=(sdl2 SDL2_image libcrypto libssl libxml-2.0)
	PACKAGES_OPT=(openal) # TODO: pocl handling

	# TODO: error checking + check if libs exist
	for pkg in ${PACKAGES[@]}; do
		LIBS="${LIBS[@]} $(pkg-config --libs "${pkg}")"
		COMMON_FLAGS="${COMMON_FLAGS} $(pkg-config --cflags "${pkg}")"
	done
	for pkg in ${PACKAGES_OPT[@]}; do
		LIBS="${LIBS[@]} $(pkg-config --libs "${pkg}")"
		COMMON_FLAGS="${COMMON_FLAGS} $(pkg-config --cflags "${pkg}")"
	done

	# libs that don't have pkg-config
	UNCHECKED_LIBS=(pthread OpenCL cuda cudart)

	# add os specific libs
	if [ $BUILD_OS == "linux" -o $BUILD_OS == "freebsd" -o $BUILD_OS == "openbsd" ]; then
		UNCHECKED_LIBS=(${UNCHECKED_LIBS[@]} Xxf86vm)
	elif [ $BUILD_OS == "mingw" -o $BUILD_OS == "cygwin" ]; then
		UNCHECKED_LIBS=(${UNCHECKED_LIBS[@]} opengl32 glu32 gdi32)
	fi
	
	# linux must also link against c++abi
	if [ $BUILD_OS == "linux" ]; then
		UNCHECKED_LIBS=(${UNCHECKED_LIBS[@]} c++abi)
	fi

	for lib in ${UNCHECKED_LIBS[@]}; do
		LIBS="${LIBS[@]} -l${lib}"
	done
	
	# mingw: "--allow-multiple-definition" is necessary, because gcc is still used as a linker
	# and will always link against libstdc++ (-> multiple definitions with libc++)
	# also note: since libc++ is linked first, libc++'s functions will be used
	if [ $BUILD_OS == "mingw" ]; then
		LDFLAGS="${LDFLAGS} -lc++.dll -Wl,--allow-multiple-definition"
	fi
else
	# on osx/ios: assume everything is installed, pkg-config doesn't really exist
	INCLUDES="${INCLUDES} -isystem /opt/X11/include"
	INCLUDES="${INCLUDES} -isystem /usr/include/libxml2"
	INCLUDES="${INCLUDES} -isystem /usr/local/opt/openssl/include"
	INCLUDES="${INCLUDES} -iframework /Library/Frameworks"
	LDFLAGS="${LDFLAGS} -L/opt/X11/lib -L/usr/local/opt/openssl/lib"
	
	# rpath voodoo
	LDFLAGS="${LDFLAGS} -install_name @rpath/${TARGET_BIN_NAME}"
	LDFLAGS="${LDFLAGS} -Xlinker -rpath -Xlinker /usr/local/lib"
	LDFLAGS="${LDFLAGS} -Xlinker -rpath -Xlinker /usr/lib"
	LDFLAGS="${LDFLAGS} -Xlinker -rpath -Xlinker @loader_path/../Resources"
	LDFLAGS="${LDFLAGS} -Xlinker -rpath -Xlinker @loader_path/../Frameworks"
	LDFLAGS="${LDFLAGS} -Xlinker -rpath -Xlinker /Library/Frameworks"
	
	# probably necessary
	LDFLAGS="${LDFLAGS} -fobjc-link-runtime"
	
	# frameworks and libs
	LDFLAGS="${LDFLAGS} -framework SDL2 -framework SDL2_image"
	# TODO: proper conf handling
	LDFLAGS="${LDFLAGS} -weak_framework CUDA"
	LDFLAGS="${LDFLAGS} -lcrypto -lssl"
	LDFLAGS="${LDFLAGS} -lxml2"
	LDFLAGS="${LDFLAGS} -framework OpenALSoft"
	# system frameworks
	LDFLAGS="${LDFLAGS} -framework ApplicationServices -framework AppKit -framework Cocoa -framework OpenCL -framework OpenGL"
fi

# just in case, also add these rather default ones (should also go after all previous libs/includes,
# in case a local or otherwise set up lib is overwriting a system lib and should be used instead)
LDFLAGS="${LDFLAGS} -L/usr/lib -L/usr/local/lib"
INCLUDES="${INCLUDES} -isystem /usr/include -isystem /usr/local/include"

# checks if any source files have updated (are newer than the target binary)
# if so, this increments the build version by one (updates the header file)
info "build version update ..."
. ./floor/build_version.sh

# version of the target (preprocess the floor version header, grep the version defines, transform them to exports and eval)
eval $(${CXX} -E -dM -I. floor/floor_version.hpp 2>&1 | grep -E "define FLOOR_(MAJOR|MINOR|REVISION|DEV_STAGE|BUILD)_VERSION " | sed -E "s/.*define (.*) [\"]*([^ \"]*)[\"]*/export \1=\2/g")
TARGET_VERSION="${FLOOR_MAJOR_VERSION}.${FLOOR_MINOR_VERSION}.${FLOOR_REVISION_VERSION}"
TARGET_FULL_VERSION="${TARGET_VERSION}${FLOOR_DEV_STAGE_VERSION}-${FLOOR_BUILD_VERSION}"
info "building ${TARGET_NAME} v${TARGET_FULL_VERSION}"

##########################################
# flags

# set up initial c++, c and linker flags
CXXFLAGS="${CXXFLAGS} -std=gnu++14 -stdlib=libc++"
CFLAGS="${CFLAGS} -std=gnu11"
LDFLAGS="${LDFLAGS} -dynamiclib"
LDFLAGS="${LDFLAGS} -stdlib=libc++"
LDFLAGS="${LDFLAGS} -fvisibility=default"

# c++ and c flags that apply to all build configurations
COMMON_FLAGS="${COMMON_FLAGS} -ffast-math -fstrict-aliasing"

# debug flags, only used in the debug target
DEBUG_FLAGS="-O0 -gdwarf-2 -DFLOOR_DEBUG=1 -DDEBUG"

# release mode flags/optimizations
# TODO: sse/avx selection/config? default to sse4.1 for now (core2)
# TODO: also add -mtune option
REL_FLAGS="-Ofast -funroll-loops -msse4.1"
REL_FLAGS="${REL_FLAGS} -mllvm -force-vector-width=4 -mllvm -force-vector-unroll=4"

# additional optimizations (used in addition to REL_CXX_FLAGS)
REL_OPT_FLAGS="-flto"
REL_OPT_LD_FLAGS="-flto"

# osx/ios: set min version
if [ $BUILD_OS == "osx" -o $BUILD_OS == "ios" ]; then
	if [ $BUILD_OS == "osx" ]; then
		COMMON_FLAGS="${COMMON_FLAGS} -mmacosx-version-min=10.7"
	else
		COMMON_FLAGS="${COMMON_FLAGS} -miphoneos-version-min=7.0"
	fi
	
	# set lib version
	LDFLAGS="${LDFLAGS} -compatibility_version ${TARGET_VERSION} -current_version ${TARGET_VERSION}"
fi

# defines
COMMON_FLAGS="${COMMON_FLAGS} -DFLOOR_EXPORT=1 -DFLOOR_CUDA_CL=1 -DTCC_LIB_ONLY=1 -DPLATFORM_X64"
#TODO: handle PLATFORM_X32

# hard-mode c++ ;) TODO: clean this up + explanations
WARNINGS="-Weverything -Wno-gnu -Wno-c++98-compat"
WARNINGS="${WARNINGS} -Wno-c++98-compat-pedantic -Wno-c99-extensions"
WARNINGS="${WARNINGS} -Wno-header-hygiene -Wno-documentation"
WARNINGS="${WARNINGS} -Wno-system-headers -Wno-global-constructors -Wno-padded"
WARNINGS="${WARNINGS} -Wno-packed -Wno-switch-enum -Wno-exit-time-destructors"
WARNINGS="${WARNINGS} -Wno-unknown-warning-option -Wno-nested-anon-types"
WARNINGS="${WARNINGS} -Wno-old-style-cast -Wno-date-time"
COMMON_FLAGS="${COMMON_FLAGS} ${WARNINGS}"

# diagnostics
COMMON_FLAGS="${COMMON_FLAGS} -fdiagnostics-show-note-include-stack -fmessage-length=0 -fmacro-backtrace-limit=0"
COMMON_FLAGS="${COMMON_FLAGS} -fparse-all-comments -fno-elide-type -fdiagnostics-show-template-tree"

# includes + replace all "-I"s with "-isystem"s so that we don't get warnings in external headers
COMMON_FLAGS="${COMMON_FLAGS} ${INCLUDES}"
COMMON_FLAGS=$(echo "${COMMON_FLAGS}" | sed -E "s/-I/-isystem /g")
COMMON_FLAGS="${COMMON_FLAGS} -I."

# finally: add all common c++ and c flags/options
CXXFLAGS="${CXXFLAGS} ${COMMON_FLAGS}"
CFLAGS="${CFLAGS} ${COMMON_FLAGS}"

##########################################
# targets and building

# get all source files (c++/c/objective-c++/objective-c) and create build folders
for dir in ${SRC_SUB_DIRS[@]}; do
	# source files
	CPP_FILES=(${CPP_FILES[@]} $(find ${dir} -type f -depth 1 -name '*.cpp'))
	C_FILES=(${C_FILES[@]} $(find ${dir} -type f -depth 1 -name '*.c'))
	OBJCPP_FILES=(${OBJCPP_FILES[@]} $(find ${dir} -type f -depth 1 -name '*.mm'))
	OBJC_FILES=(${OBJC_FILES[@]} $(find ${dir} -type f -depth 1 -name '*.m'))
	
	# create resp. build folder
	mkdir -p ${BUILD_DIR}/${dir}
done

# set flags depending on the build mode, or make a clean exit
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
	"clean")
		# delete the target binary and the complete build folder (all object files)
		info "cleaning ..."
		rm -f ${TARGET_BIN}
		rm -Rf ${BUILD_DIR}
		exit 0
		;;
	*)
		error "unknown build mode: ${BUILD_MODE}"
		;;
esac

if [ ${BUILD_VERBOSE} -gt 1 ]; then
	CXXFLAGS="${CXXFLAGS} -v"
	CFLAGS="${CXXFLAGS} -v"
	LDFLAGS="${CXXFLAGS} -v"
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

# build the target
OBJ_FILES=()
for source_file in ${CPP_FILES[@]}; do
	info "building ${source_file}"
	OBJ_FILES=(${OBJ_FILES[@]} ${BUILD_DIR}/${source_file}.o)
	verbose "${CXX} ${CXXFLAGS} -c ${source_file} -o ${BUILD_DIR}/${source_file}.o -MMD -MT deps -MF ${BUILD_DIR}/${source_file}.d"
	${CXX} ${CXXFLAGS} -c ${source_file} -o ${BUILD_DIR}/${source_file}.o -MMD -MT deps -MF ${BUILD_DIR}/${source_file}.d
done
for source_file in ${C_FILES[@]}; do
	info "building ${source_file}"
	OBJ_FILES=(${OBJ_FILES[@]} ${BUILD_DIR}/${source_file}.o)
	verbose "${CC} ${CFLAGS} -c ${source_file} -o ${BUILD_DIR}/${source_file}.o -MMD -MT deps -MF ${BUILD_DIR}/${source_file}.d"
	${CC} ${CFLAGS} -c ${source_file} -o ${BUILD_DIR}/${source_file}.o -MMD -MT deps -MF ${BUILD_DIR}/${source_file}.d
done
for source_file in ${OBJCPP_FILES[@]}; do
	info "building ${source_file}"
	OBJ_FILES=(${OBJ_FILES[@]} ${BUILD_DIR}/${source_file}.o)
	verbose "${CXX} -x objective-c++ ${CXXFLAGS} -c ${source_file} -o ${BUILD_DIR}/${source_file}.o -MMD -MT deps -MF ${BUILD_DIR}/${source_file}.d"
	${CXX} -x objective-c++ ${CXXFLAGS} -c ${source_file} -o ${BUILD_DIR}/${source_file}.o -MMD -MT deps -MF ${BUILD_DIR}/${source_file}.d
done
for source_file in ${OBJC_FILES[@]}; do
	info "building ${source_file}"
	OBJ_FILES=(${OBJ_FILES[@]} ${BUILD_DIR}/${source_file}.o)
	verbose "${CC} -x objective-c ${CFLAGS} -c ${source_file} -o ${BUILD_DIR}/${source_file}.o -MMD -MT deps -MF ${BUILD_DIR}/${source_file}.d"
	${CC} -x objective-c ${CFLAGS} -c ${source_file} -o ${BUILD_DIR}/${source_file}.o -MMD -MT deps -MF ${BUILD_DIR}/${source_file}.d
done

info "linking ..."
verbose "${CXX} -o ${TARGET_BIN} ${OBJ_FILES[@]} ${LDFLAGS}"
${CXX} -o ${TARGET_BIN} ${OBJ_FILES[@]} ${LDFLAGS}
info "built ${TARGET_NAME} v${TARGET_FULL_VERSION}"
