#!/usr/bin/env bash

# NOTE: this script is intended for deploying a locally built toolchain, i.e. one that has manually been built via ./build.sh,
#       for deploying a packaged toolchain, deploy_pkg.sh should be used instead

RELEASE=13.0.0
RELEASE_VERSION_NAME=130000

##########################################
# helper functions
info() {
	if [ -z "$NO_COLOR" ]; then
		printf "\033[1;32m>> $@ \033[m\n"
	else
		printf ">> $@\n"
	fi
}
error() {
	if [ -z "$NO_COLOR" ]; then
		printf "\033[1;31m>> $@ \033[m\n"
	else
		printf "error: $@\n"
	fi
	exit -1
}

# check on which platform we're deploying
DEPLOY_PLATFORM=$(uname | tr [:upper:] [:lower:])
DEPLOY_OS="unix"
case ${DEPLOY_PLATFORM} in
	"darwin"|"linux"|"freebsd"|"openbsd")
		DEPLOY_OS="unix"
		;;
	"cygwin"*|"mingw"*)
		DEPLOY_OS="windows"
		;;
	*)
		warning "unknown platform - trying to continue! ${DEPLOY_PLATFORM}"
		;;
esac

if [ $DEPLOY_OS == "windows" ]; then
	id -G | grep -qE "\<(114|544)\>"
	if [ $? -ne 0 ]; then
		error "must run as root/Administrator on Windows"
	fi
fi

##########################################
# arg handling / default args
DEPLOY_TOOLCHAIN_FOLDER="toolchain"
DEPLOY_FLOOR_ROOT_FOLDER="/opt/floor"
if [ $DEPLOY_OS == "windows" ]; then
	DEPLOY_FLOOR_ROOT_FOLDER="${ProgramW6432}/floor"
fi
DEPLOY_FLOOR_INCLUDE_FOLDER="../include"
DEPLOY_LLVM_BUILD_FOLDER="build"
DEPLOY_SPIRV_TOOLS_BUILD_FOLDER="SPIRV-Tools/build"
DEPLOY_LIBCXX_INCLUDE_FOLDER="libcxx/include"
DEPLOY_SYMLINK=1
DEPLOY_SYMLINK_FLOOR_INCLUDE=1
DEPLOY_DRY_RUN=0

# parse args
get_arg() {
	echo "$@" | sed -E "s/([^=]+)=(.*)/\2/g"
}

for arg in "$@"; do
	case $arg in
		"help"|"-help"|"--help")
			info "deploy script usage:"
			echo ""
			echo "deploy options:"
			echo "	toolchain-folder=<folder>             toolchain target folder name (default: ${DEPLOY_TOOLCHAIN_FOLDER})"
			echo "	floor-root-folder=<folder>            libfloor target root folder (default: ${DEPLOY_FLOOR_ROOT_FOLDER})"
			echo "	floor-include-folder=<folder>         libfloor include folder, relative from the toolchain folder, or absolute (default: ${DEPLOY_FLOOR_INCLUDE_FOLDER})"
			echo "	llvm-build-folder=<folder>            clang/LLVM build folder (default: ${DEPLOY_LLVM_BUILD_FOLDER})"
			echo "	spirv-tools-build-folder=<folder>     SPIR-V Tools build folder (default: ${DEPLOY_SPIRV_TOOLS_BUILD_FOLDER})"
			echo "	libcxx-include-folder=<folder>        libc++ include folder (default: ${DEPLOY_LIBCXX_INCLUDE_FOLDER})"
			echo "	symlink                               symlink all clang/LLVM/libc++/SPIR-V files and folders (default)"
			echo "	copy                                  copy all clang/LLVM/libc++/SPIR-V files and folders to the target toolchain folder"
			echo "	copy-floor-include                    copy floor include files instead of symlinking floor"
			echo "	dry-run                               just do a dry-run, don't create/copy/symlink any files/folders"
			echo ""
			echo "NOTE: if a toolchain should already exist in the target folder, it will be removed!"
			echo ""
			exit 0
			;;
		*)
			;; # ignore
	esac
done

for arg in "$@"; do
	case $arg in
		"help"|"-help"|"--help")
			;; # ignore
		"toolchain-folder="*)
			DEPLOY_TOOLCHAIN_FOLDER=$(get_arg $arg)
			;;
		"floor-root-folder="*)
			DEPLOY_FLOOR_ROOT_FOLDER=$(get_arg $arg)
			;;
		"floor-include-folder="*)
			DEPLOY_FLOOR_INCLUDE_FOLDER=$(get_arg $arg)
			;;
		"llvm-build-folder="*)
			DEPLOY_LLVM_BUILD_FOLDER=$(get_arg $arg)
			;;
		"spirv-tools-build-folder="*)
			DEPLOY_SPIRV_TOOLS_BUILD_FOLDER=$(get_arg $arg)
			;;
		"libcxx-include-folder="*)
			DEPLOY_LIBCXX_INCLUDE_FOLDER=$(get_arg $arg)
			;;
		"symlink")
			DEPLOY_SYMLINK=1
			;;
		"copy")
			DEPLOY_SYMLINK=0
			;;
		"copy-floor-include")
			DEPLOY_SYMLINK_FLOOR_INCLUDE=0
			;;
		"dry-run")
			DEPLOY_DRY_RUN=1
			;;
		*)
			error "unknown argument: ${arg}"
			;;
	esac
done

# make absolute collapsed paths
collapse_path() {
	if [ $DEPLOY_OS != "windows" ]; then
		echo "$(cd "$1" 2>/dev/null && pwd)"
	else
		echo "$1"
	fi
}
make_win_path() {
	if [ ${1::1} == '/' ]; then
		# on Windows: replace /letter/ with Letter:/
		win_path=$(echo "$1" | sed -E "s/\/([a-zA-Z]+)\/(.*)/\1:\/\2/")
		echo "$win_path"
		exit 0
	fi
	echo "$1"
}
make_abs_path() {
	if [ "${#1}" -eq 0 ]; then # just in case
		echo "$(pwd)/"
		exit 0
	fi

	if [ ${1::1} == '/' ]; then
		if [ $DEPLOY_OS != "windows" ]; then
			# assume this is already an absolute path
			echo "$1"
		else
			echo $(make_win_path "$1")
		fi
	elif [ $DEPLOY_OS == "windows" -a \( ${1:1:2} == ":\\" -o ${1:1:2} == ":/" \) ]; then
		# assume this is already an absolute path
		echo $1
	else
		if [ $DEPLOY_OS != "windows" ]; then
			echo "$(pwd)/$1"
		else
			echo $(make_win_path "$(pwd)/$1")
		fi
	fi
}

DEPLOY_FLOOR_ROOT_FOLDER=$(make_abs_path "${DEPLOY_FLOOR_ROOT_FOLDER}")
DEPLOY_LLVM_BUILD_FOLDER=$(make_abs_path "${DEPLOY_LLVM_BUILD_FOLDER}")
DEPLOY_SPIRV_TOOLS_BUILD_FOLDER=$(make_abs_path "${DEPLOY_SPIRV_TOOLS_BUILD_FOLDER}")
DEPLOY_LIBCXX_INCLUDE_FOLDER=$(make_abs_path "${DEPLOY_LIBCXX_INCLUDE_FOLDER}")

if [ ! -e ${DEPLOY_FLOOR_ROOT_FOLDER} ]; then
	if [ ${DEPLOY_DRY_RUN} -eq 0 ]; then
		info "floor root folder ${DEPLOY_FLOOR_ROOT_FOLDER} is missing, creating now ..."
		mkdir -p "${DEPLOY_FLOOR_ROOT_FOLDER}"
		if [ $? -ne 0 ]; then
			error "failed to create floor root folder - maybe run with sudo?"
		fi
	else
		error "floor root folder does not exists: ${DEPLOY_FLOOR_ROOT_FOLDER}"
	fi
fi
DEPLOY_FLOOR_ROOT_FOLDER=$(collapse_path "$(make_abs_path "${DEPLOY_FLOOR_ROOT_FOLDER}")")
DEPLOY_TARGET="${DEPLOY_FLOOR_ROOT_FOLDER}/${DEPLOY_TOOLCHAIN_FOLDER}"
if [ ${#DEPLOY_TARGET} -eq 0 ]; then
	error "empty or non-existing deploy target path"
fi

DEPLOY_FLOOR_INCLUDE_FOLDER=$(make_abs_path "${DEPLOY_FLOOR_ROOT_FOLDER}/${DEPLOY_TOOLCHAIN_FOLDER}/${DEPLOY_FLOOR_INCLUDE_FOLDER}")
if [ ${#DEPLOY_FLOOR_INCLUDE_FOLDER} -eq 0 ]; then
	error "empty or non-existing floor include path"
fi

# print all info
info "deploying with "$(if [ ${DEPLOY_SYMLINK} -eq 1 ]; then printf "symlink"; else printf "copy"; fi)" at: ${DEPLOY_TARGET}"
info "floor root folder: ${DEPLOY_FLOOR_ROOT_FOLDER}"
info "floor include folder: ${DEPLOY_FLOOR_INCLUDE_FOLDER}"
info "LLVM build folder: ${DEPLOY_LLVM_BUILD_FOLDER}"
info "SPIRV-Tools build folder: ${DEPLOY_SPIRV_TOOLS_BUILD_FOLDER}"
info "libc++ include folder: ${DEPLOY_LIBCXX_INCLUDE_FOLDER}"

##########################################
# deploy
info "deploying ..."

if [ ${DEPLOY_DRY_RUN} -eq 0 ]; then
	# remove existing toolchain
	if [[ -e "${DEPLOY_TARGET}" ]]; then
		info "removing existing toolchain at ${DEPLOY_TARGET} ..."
		rm -R "${DEPLOY_TARGET}"
	fi
	if [[ -e "${DEPLOY_TARGET}" ]]; then
		error "failed to remove existing toolchain - maybe run with sudo?"
	fi

	# create target path
	mkdir -p "${DEPLOY_TARGET}/bin"
	if [ $? -ne 0 ]; then
		error "failed to create target folder - maybe run with sudo?"
	fi
fi

deploy_file() {
	cmd=""
	if [[ $3 -eq 1 ]]; then
		if [ $DEPLOY_OS != "windows" ]; then
			cmd="ln -sf $2/$1 ${DEPLOY_TARGET}/$3/$1"
		else
			cmd="cmd <<< 'mklink \"${DEPLOY_TARGET}/$3/$1.exe\" \"$2/$1.exe\"'"
		fi
	else
		cmd="cp \"$2/$1\" \"${DEPLOY_TARGET}/$3/$1\""
	fi
	info "${cmd}"
	if [ ${DEPLOY_DRY_RUN} -eq 0 ]; then
		eval ${cmd}
	fi
}
deploy_folder() {
	cmd=""
	if [[ $3 -eq 1 ]]; then
		if [ $DEPLOY_OS != "windows" ]; then
			cmd="ln -sf $1 $2"
		else
			cmd="cmd <<< 'mklink /D \"$2\" \"$1\"'"
		fi
	else
		cmd="cp -R \"$1\" \"$2\""
	fi
	info "${cmd}"
	if [ ${DEPLOY_DRY_RUN} -eq 0 ]; then
		eval ${cmd}
	fi
}

deploy_file "clang" "${DEPLOY_LLVM_BUILD_FOLDER}/bin" "bin" ${DEPLOY_SYMLINK}
deploy_file "llvm-as" "${DEPLOY_LLVM_BUILD_FOLDER}/bin" "bin" ${DEPLOY_SYMLINK}
deploy_file "llvm-dis" "${DEPLOY_LLVM_BUILD_FOLDER}/bin" "bin" ${DEPLOY_SYMLINK}
deploy_file "llvm-spirv" "${DEPLOY_LLVM_BUILD_FOLDER}/bin" "bin" ${DEPLOY_SYMLINK}
deploy_file "metallib-dis" "${DEPLOY_LLVM_BUILD_FOLDER}/bin" "bin" ${DEPLOY_SYMLINK}
deploy_file "spirv-as" "${DEPLOY_SPIRV_TOOLS_BUILD_FOLDER}/tools" "bin" ${DEPLOY_SYMLINK}
deploy_file "spirv-dis" "${DEPLOY_SPIRV_TOOLS_BUILD_FOLDER}/tools" "bin" ${DEPLOY_SYMLINK}
deploy_file "spirv-opt" "${DEPLOY_SPIRV_TOOLS_BUILD_FOLDER}/tools" "bin" ${DEPLOY_SYMLINK}
deploy_file "spirv-val" "${DEPLOY_SPIRV_TOOLS_BUILD_FOLDER}/tools" "bin" ${DEPLOY_SYMLINK}

deploy_folder "${DEPLOY_LLVM_BUILD_FOLDER}/lib/clang/${RELEASE}/include" "${DEPLOY_TARGET}/clang" ${DEPLOY_SYMLINK}
deploy_folder "${DEPLOY_LIBCXX_INCLUDE_FOLDER}" "${DEPLOY_TARGET}/libcxx" ${DEPLOY_SYMLINK}
deploy_folder "${DEPLOY_FLOOR_INCLUDE_FOLDER}" "${DEPLOY_TARGET}/floor" ${DEPLOY_SYMLINK_FLOOR_INCLUDE}

# on Windows, also deploy/copy required dlls
if [ $DEPLOY_OS == "windows" ]; then
	DEPLOY_DEP_AS_BIN=1
	deploy_file "libgcc_s_seh-1.dll" "/mingw64/bin" "bin" ${DEPLOY_DEP_AS_BIN}
	deploy_file "libstdc++-6.dll" "/mingw64/bin" "bin" ${DEPLOY_DEP_AS_BIN}
	deploy_file "libwinpthread-1.dll" "/mingw64/bin" "bin" ${DEPLOY_DEP_AS_BIN}
	deploy_file "zlib1.dll" "/mingw64/bin" "bin" ${DEPLOY_DEP_AS_BIN}
	deploy_file "libz3.dll" "/mingw64/bin" "bin" ${DEPLOY_DEP_AS_BIN}
	deploy_file "libgomp-1.dll" "/mingw64/bin" "bin" ${DEPLOY_DEP_AS_BIN}
fi

info "done"
