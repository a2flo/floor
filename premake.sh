#!/bin/sh

BUILD_OS="unknown"
BUILD_PLATFORM="x32"
BUILD_MAKE="make"
BUILD_MAKE_PLATFORM="32"
BUILD_ARGS=""
BUILD_CPU_COUNT=1
BUILD_USE_CLANG=1

for arg in "$@"; do
	case $arg in
		"gcc")
			BUILD_ARGS=${BUILD_ARGS}" --gcc"
			BUILD_USE_CLANG=0
			;;
		"cuda")
			BUILD_ARGS=${BUILD_ARGS}" --cuda"
			;;
		"pocl")
			BUILD_ARGS=${BUILD_ARGS}" --pocl"
			;;
		"windows")
			BUILD_ARGS=${BUILD_ARGS}" --windows"
			;;
		"cl-profiling")
			BUILD_ARGS=${BUILD_ARGS}" --cl-profiling"
			;;
		*)
			;;
	esac
done

if [ $BUILD_USE_CLANG == 1 ]; then
	BUILD_ARGS=${BUILD_ARGS}" --clang"
fi

case $( uname | tr [:upper:] [:lower:] ) in
	"darwin")
		BUILD_OS="macosx"
		BUILD_CPU_COUNT=$(sysctl -n hw.ncpu)
		;;
	"linux")
		BUILD_OS="linux"
		# note that this includes hyper-threading and multi-socket systems
		BUILD_CPU_COUNT=$(cat /proc/cpuinfo | grep "processor" | wc -l)
		;;
	[a-z0-9]*"bsd")
		BUILD_OS="bsd"
		BUILD_MAKE="gmake"
		BUILD_CPU_COUNT=$(sysctl -n hw.ncpu)
		;;
	"cygwin"*)
		BUILD_OS="windows"
		BUILD_ARGS=${BUILD_ARGS}" --env cygwin"
		BUILD_CPU_COUNT=$(env | grep 'NUMBER_OF_PROCESSORS' | sed -E 's/.*=([:digit:]*)/\1/g')
		;;
	"mingw"*)
		BUILD_OS="windows"
		BUILD_ARGS=${BUILD_ARGS}" --env mingw"
		BUILD_CPU_COUNT=$(env | grep 'NUMBER_OF_PROCESSORS' | sed -E 's/.*=([:digit:]*)/\1/g')
		;;
	*)
		echo "unknown operating system - exiting"
		exit
		;;
esac

BUILD_PLATFORM_TEST_STRING=""
if [ $BUILD_OS != "windows" ]; then
	BUILD_PLATFORM_TEST_STRING=$( uname -m )
else
	BUILD_PLATFORM_TEST_STRING=$( gcc -dumpmachine | sed "s/-.*//" )
fi

case $BUILD_PLATFORM_TEST_STRING in
	"i386"|"i486"|"i586"|"i686")
		BUILD_PLATFORM="x32"
		BUILD_MAKE_PLATFORM="32"
		BUILD_ARGS=${BUILD_ARGS}" --platform x32"
		;;
	"x86_64"|"amd64")
		BUILD_PLATFORM="x64"
		BUILD_MAKE_PLATFORM="64"
		BUILD_ARGS=${BUILD_ARGS}" --platform x64"
		;;
	*)
		echo "unknown architecture - using "${BUILD_PLATFORM}
		exit;;
esac

echo "using: premake4 --cc=gcc --os="${BUILD_OS}" gmake"${BUILD_ARGS}

premake4 --cc=gcc --os=${BUILD_OS} gmake ${BUILD_ARGS}
sed -i -e 's/\${MAKE}/\${MAKE} -j '${BUILD_CPU_COUNT}'/' Makefile

if [ $BUILD_USE_CLANG == 1 ]; then
	# this seems to be the most portable way of inserting chars in front of a file
	# note that "sed -i "1i ..." file" is not portable!
	mv Makefile Makefile.tmp
	echo "export CC=clang" > Makefile
	echo "export CXX=clang++" >> Makefile
	cat Makefile.tmp >> Makefile
	rm Makefile.tmp
fi
sed -i -e 's/-std=c++11/-std=c11/g' libfloor.make
sed -i -e 's/-Wall -stdlib=libc++/-Wall/g' libfloor.make
sed -i -e 's/CXXFLAGS  += $(CFLAGS)/CXXFLAGS  += $(CFLAGS) -std=c++11 -stdlib=libc++/g' libfloor.make

chmod +x floor/build_version.sh

echo ""
echo "############################################################################"
echo "#"
echo "# NOTE: use '"${BUILD_MAKE}"' to build floor"
echo "#"
echo "############################################################################"
echo ""

echo ""
echo "############################################################################"
echo "# if you have just cloned floor, but haven't cloned its submodules yet:"
echo "#"
echo "#   run: git submodule init && git submodule update && ./premake.sh"
echo "#   and on every submodule change: git submodule update"
echo "#"
echo "############################################################################"
echo ""
