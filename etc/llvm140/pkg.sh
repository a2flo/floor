#!/usr/bin/env bash

RELEASE=14.0.0
RELEASE_VERSION_NAME=140000

VERSION=$(date +%Y%m%d_%H%M)
TOOLCHAIN_NAME="toolchain_${RELEASE_VERSION_NAME}_${VERSION}"
mkdir ${TOOLCHAIN_NAME}
cd ${TOOLCHAIN_NAME}

cp ../deploy_pkg.sh .
chmod +x deploy_pkg.sh

mkdir bin
cp ../build/bin/clang bin/
cp ../build/bin/llvm-dis bin/
cp ../build/bin/llvm-as bin/
cp ../build/bin/llvm-spirv bin/
cp ../build/bin/metallib-dis bin/
cp ../SPIRV-Tools/build/tools/spirv-as bin/
cp ../SPIRV-Tools/build/tools/spirv-dis bin/
cp ../SPIRV-Tools/build/tools/spirv-opt bin/
cp ../SPIRV-Tools/build/tools/spirv-val bin/

cp -R ../build/lib/clang/${RELEASE}/include clang

cp -R ../llvm/libcxx/include libcxx
cp ../llvm/libcxx/{LICENSE.TXT,CREDITS.TXT} libcxx/

mkdir -p floor/floor
FLOOR_SUB_DIRS="audio compute compute/device compute/cuda compute/host compute/metal compute/opencl compute/vulkan constexpr core floor lang math net threading"
for dir in ${FLOOR_SUB_DIRS}; do
	# create resp. folder
	mkdir -p floor/floor/${dir}

	# copy include files
	FLOOR_INCL_FILES=""
	FLOOR_INCL_FILES="${FLOOR_INCL_FILES} $(find ../../../${dir} -maxdepth 1 -type f -name '*.hpp' | grep -v "\._")"
	FLOOR_INCL_FILES="${FLOOR_INCL_FILES} $(find ../../../${dir} -maxdepth 1 -type f -name '*.h' | grep -v "\._")"
	for file in ${FLOOR_INCL_FILES}; do
		cp $file floor/floor/${dir}/
	done
done

BUILD_PLATFORM=$(uname | tr [:upper:] [:lower:])
case ${BUILD_PLATFORM} in
	"mingw"*)
		# also copy all necessary .dll files
		cp "/mingw64/bin/libgcc_s_seh-1.dll" bin/
		cp "/mingw64/bin/libstdc++-6.dll" bin/
		cp "/mingw64/bin/libwinpthread-1.dll" bin/
		cp "/mingw64/bin/zlib1.dll" bin/
		cp "/mingw64/bin/libz3.dll" bin/
		cp "/mingw64/bin/libgomp-1.dll" bin/
		;;
	*)
		;;
esac

cd ..


zip -qr ${TOOLCHAIN_NAME}.zip ${TOOLCHAIN_NAME}
zip_ret_code=$?
if [ ${zip_ret_code} -ne 0 ]; then
	echo "failed to create toolchain archive!"
	exit 1
else
	echo "sucessfully created toolchain archive: ${TOOLCHAIN_NAME}.zip"
fi
