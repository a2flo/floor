#!/bin/sh

FLOOR_PLATFORM="x86"
FLOOR_PLATFORM_TEST_STRING=""
if [[ $( uname -o ) != "Msys" ]]; then
	FLOOR_PLATFORM_TEST_STRING=$( uname -m )
else
	FLOOR_PLATFORM_TEST_STRING=$( gcc -dumpmachine | sed "s/-.*//" )
fi

case $FLOOR_PLATFORM_TEST_STRING in
	"i386"|"i486"|"i586"|"i686")
		FLOOR_PLATFORM="x86"
		;;
	"x86_64"|"amd64")
		FLOOR_PLATFORM="x64"
		;;
	*)
		echo "unknown architecture - using "${FLOOR_PLATFORM}
		exit;;
esac

declare -a paths=( cl core cuda floor hash net tccpp threading )
case $( uname | tr [:upper:] [:lower:] ) in
	"linux"|[a-z0-9]*"BSD")
		FLOOR_INCLUDE_PATH="/usr/local/include"
		FLOOR_LIB_PATH="/usr/local/lib"
	
		# remove old files and folders
		rm -Rf ${FLOOR_INCLUDE_PATH}/floor
		rm -f ${FLOOR_LIB_PATH}/libfloor.so
		rm -f ${FLOOR_LIB_PATH}/libfloord.so
		rm -f ${FLOOR_LIB_PATH}/libfloor.a
		rm -f ${FLOOR_LIB_PATH}/libfloord.a
		
		# create/copy new files and folders
		mkdir ${FLOOR_INCLUDE_PATH}/floor
		for val in ${paths[@]}; do
			mkdir -p ${FLOOR_INCLUDE_PATH}/floor/${val}
		done
		cp *.h ${FLOOR_INCLUDE_PATH}/floor/ 2>/dev/null
		cp *.hpp ${FLOOR_INCLUDE_PATH}/floor/ 2>/dev/null
		for val in ${paths[@]}; do
			cp ${val}/*.h ${FLOOR_INCLUDE_PATH}/floor/${val}/ 2>/dev/null
			cp ${val}/*.hpp ${FLOOR_INCLUDE_PATH}/floor/${val}/ 2>/dev/null
		done
		
		cp bin/libfloor.so ${FLOOR_LIB_PATH}/ 2>/dev/null
		cp bin/libfloord.so ${FLOOR_LIB_PATH}/ 2>/dev/null
		cp bin/libfloor.a ${FLOOR_LIB_PATH}/ 2>/dev/null
		cp bin/libfloord.a ${FLOOR_LIB_PATH}/ 2>/dev/null
		;;
	"mingw"*)
		FLOOR_MINGW_ROOT="/c/mingw"
		if [[ $FLOOR_PLATFORM == "x64" ]]; then
			FLOOR_MINGW_ROOT="/c/mingw64"
			if [[ $MINGW_ROOT ]]; then
				FLOOR_MINGW_ROOT=$MINGW_ROOT
			fi
			if [[ $MINGW64_ROOT ]]; then
				FLOOR_MINGW_ROOT=$MINGW64_ROOT
			fi
		else
			if [[ $MINGW_ROOT ]]; then
				FLOOR_MINGW_ROOT=$MINGW_ROOT
			fi
			if [[ $MINGW32_ROOT ]]; then
				FLOOR_MINGW_ROOT=$MINGW32_ROOT
			fi
		fi
		echo "# installing to "$FLOOR_MINGW_ROOT" ..."
		FLOOR_INCLUDE_PATH=$FLOOR_MINGW_ROOT"/msys/include"
		FLOOR_BIN_PATH=$FLOOR_MINGW_ROOT"/bin"
		FLOOR_LIB_PATH=$FLOOR_MINGW_ROOT"/msys/lib"

		# create bin/export folder if it doesn't exist
		if [[ ! -d "${FLOOR_BIN_PATH}" ]]; then
			mkdir -p ${FLOOR_BIN_PATH}
		fi
	
		# remove old files and folders
		rm -Rf ${FLOOR_INCLUDE_PATH}/floor
		rm -f ${FLOOR_BIN_PATH}/floor.dll
		rm -f ${FLOOR_BIN_PATH}/floord.dll
		rm -f ${FLOOR_LIB_PATH}/libfloor.a
		rm -f ${FLOOR_LIB_PATH}/libfloord.a
		
		# create/copy new files and folders
		mkdir ${FLOOR_INCLUDE_PATH}/floor
		for val in ${paths[@]}; do
			mkdir -p ${FLOOR_INCLUDE_PATH}/floor/${val}
		done
		cp *.h ${FLOOR_INCLUDE_PATH}/floor/ 2>/dev/null
		cp *.hpp ${FLOOR_INCLUDE_PATH}/floor/ 2>/dev/null
		for val in ${paths[@]}; do
			cp ${val}/*.h ${FLOOR_INCLUDE_PATH}/floor/${val}/ 2>/dev/null
			cp ${val}/*.hpp ${FLOOR_INCLUDE_PATH}/floor/${val}/ 2>/dev/null
		done
		
		cp bin/floor.dll ${FLOOR_BIN_PATH}/ 2>/dev/null
		cp bin/floord.dll ${FLOOR_BIN_PATH}/ 2>/dev/null
		cp bin/libfloor.a ${FLOOR_LIB_PATH}/ 2>/dev/null
		cp bin/libfloord.a ${FLOOR_LIB_PATH}/ 2>/dev/null
		;;
	*)
		echo "unknown operating system - exiting"
		exit
		;;
esac

echo ""
echo "#########################################################"
echo "# floor has been installed!"
echo "#########################################################"
echo ""
