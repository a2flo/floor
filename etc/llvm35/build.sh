#!/bin/sh

RELEASE=3.5.1

# download src archives (if not already done)
if [ ! -f llvm-${RELEASE}.src.tar.xz ]; then
	curl -o llvm-${RELEASE}.src.tar.xz http://llvm.org/releases/${RELEASE}/llvm-${RELEASE}.src.tar.xz
fi
if [ ! -f cfe-${RELEASE}.src.tar.xz ]; then
	curl -o cfe-${RELEASE}.src.tar.xz http://llvm.org/releases/${RELEASE}/cfe-${RELEASE}.src.tar.xz
fi
if [ ! -f libcxx-${RELEASE}.src.tar.xz ]; then
	curl -o libcxx-${RELEASE}.src.tar.xz http://llvm.org/releases/${RELEASE}/libcxx-${RELEASE}.src.tar.xz
fi
if [ ! -d SPIR-Tools ]; then
	git clone https://github.com/KhronosGroup/SPIR-Tools.git
fi

# clean up prior source and build folders
rm -Rf llvm 2>/dev/null
rm -Rf libcxx 2>/dev/null
rm -Rf build 2>/dev/null

# untar and move
tar xfv llvm-${RELEASE}.src.tar.xz
tar xfv cfe-${RELEASE}.src.tar.xz
tar xfv libcxx-${RELEASE}.src.tar.xz

mv llvm-3.5.1.src llvm
mv cfe-3.5.1.src llvm/tools/clang
mv libcxx-3.5.1.src libcxx

# cp in and patch spir-encoder
SPIR_PATH=SPIR-Tools/spir-encoder/llvm_3.5_spir_encoder/
mkdir llvm/tools/spir-encoder
cp ${SPIR_PATH}/driver/*.cpp llvm/tools/spir-encoder/
cp ${SPIR_PATH}/encoder/*.cpp llvm/tools/spir-encoder/
cp ${SPIR_PATH}/encoder/*.h llvm/tools/spir-encoder/
cp ${SPIR_PATH}/README llvm/tools/spir-encoder/
cp llvm/lib/Bitcode/Writer/ValueEnumerator.h llvm/tools/spir-encoder/
printf "[component_0]\ntype = Tool\nname = spir-encoder\nparent = Tools\nrequired_libraries = IRReader\n" > llvm/tools/spir-encoder/LLVMBuild.txt
printf "LEVEL := ../..\nTOOLNAME := spir-encoder\nLINK_COMPONENTS := irreader bitreader bitwriter\nTOOL_NO_EXPORTS := 1\ninclude \$(LEVEL)/Makefile.common\n" > llvm/tools/spir-encoder/Makefile
cd llvm
patch -p1 < ../spir_encoder.patch
cd ..

# patch
cd llvm
patch -p1 < ../351_clang_llvm.patch
cd ../libcxx
patch -p1 < ../351_libcxx.patch
cd ..

# build
mkdir build
cd build

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
		echo "unknown build platform - trying to continue! ${BUILD_PLATFORM}"
		;;
esac

EXTRA_OPTIONS=""
if [ $BUILD_OS == "osx" ]; then
	EXTRA_OPTIONS="-mmacosx-version-min=10.9"
elif [ $BUILD_OS == "ios" ]; then
	EXTRA_OPTIONS="-miphoneos-version-min=8.0"
fi

CONFIG_OPTIONS=
CLANG_OPTIONS=
if [ $CXX == "clang++" ]; then
	CONFIG_OPTIONS="--enable-libcpp"
	CLANG_OPTIONS="-fvectorize -flto"
else
	if [ $BUILD_OS == "linux" ]; then
		CONFIG_OPTIONS="--prefix=/usr --sysconfdir=/etc --enable-shared --enable-libffi --enable-targets=all --disable-expensive-checks --disable-debug-runtime --with-binutils-include=/usr/include --with-python=/usr/bin/python2"
	fi
fi

CC=${CC} CXX=${CXX} ../llvm/configure ${CONFIG_OPTIONS} --enable-optimized --disable-assertions --enable-cxx11 --with-optimize-option="-Ofast -msse4.1 ${CLANG_OPTIONS} -funroll-loops -mtune=native" --with-extra-options="${EXTRA_OPTIONS}"
make -j ${BUILD_CPU_COUNT}

echo ""
echo "#########################"
echo "# clang/llvm build done #"
echo "#########################"
echo ""

#
cd ..
echo "copying clang includes to libcxx folder ..."
cp -R build/Release/lib/clang/${RELEASE}/include libcxx/clang
echo "done"
