#!/usr/bin/env bash

# NOTE: this script is intended for deploying a packaged/automatically built toolchain,
#       for deploying a locally/manually built toolchain, deploy_dev.sh should be used instead

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

##########################################
# arg handling / default args

DEPLOY_TOOLCHAIN_FOLDER="toolchain"
DEPLOY_FLOOR_ROOT_FOLDER="/opt/floor"
DEPLOY_FLOOR_INCLUDE_FOLDER="floor"
DEPLOY_SYMLINK_FLOOR_INCLUDE=0
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
			echo "	floor-include-folder=<folder>         libfloor include folder (default: ${DEPLOY_FLOOR_INCLUDE_FOLDER})"
			echo "	symlink-floor-include                 symlink the specified floor include folder instead of copying the packaged floor include files"
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
		"symlink-floor-include")
			DEPLOY_SYMLINK_FLOOR_INCLUDE=1
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
	echo $(cd $1 2>/dev/null && pwd)
}
make_abs_path() {
	if [ "${#1}" -eq 0 ]; then # just in case
		echo $(pwd)"/"
		exit 0
	fi

	if [ ${1::1} == '/' ]; then
		# assume this is already an absolute path
		echo $1
	else
		echo $(pwd)"/"$1
	fi
}

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
DEPLOY_FLOOR_ROOT_FOLDER=$(collapse_path $(make_abs_path "${DEPLOY_FLOOR_ROOT_FOLDER}"))
DEPLOY_TARGET="${DEPLOY_FLOOR_ROOT_FOLDER}/${DEPLOY_TOOLCHAIN_FOLDER}"
if [ ${#DEPLOY_TARGET} -eq 0 ]; then
	error "empty or non-existing deploy target path"
fi

# print all info
info "deploying at: ${DEPLOY_TARGET}"
info "floor root folder: ${DEPLOY_FLOOR_ROOT_FOLDER}"

##########################################
# deploy
info "deploying ..."

if [ ${DEPLOY_DRY_RUN} -eq 0 ]; then
	# remove existing toolchain
	if [ -e ${DEPLOY_TARGET} ]; then
		info "removing existing toolchain at ${DEPLOY_TARGET} ..."
		rm -R ${DEPLOY_TARGET}
	fi
	if [ -e ${DEPLOY_TARGET} ]; then
		error "failed to remove existing toolchain - maybe run with sudo?"
	fi

	# create target path
	mkdir -p ${DEPLOY_TARGET}/bin
	if [ $? -ne 0 ]; then
		error "failed to create target folder - maybe run with sudo?"
	fi
fi

deploy_file() {
	cmd="cp $2/$1 ${DEPLOY_TARGET}/$3/$1"
	info "${cmd}"
	if [ ${DEPLOY_DRY_RUN} -eq 0 ]; then
		eval ${cmd}
	fi
}
deploy_folder() {
	cmd=""
	if [ $3 -eq 1 ]; then
		cmd="ln -sf $1 $2"
	else
		cmd="cp -R $1 $2"
	fi
	info "${cmd}"
	if [ ${DEPLOY_DRY_RUN} -eq 0 ]; then
		eval ${cmd}
	fi
}

deploy_file "clang" "bin" "bin"
deploy_file "llvm-as" "bin" "bin"
deploy_file "llvm-dis" "bin" "bin"
deploy_file "llvm-spirv" "bin" "bin"
deploy_file "metallib-dis" "bin" "bin"
deploy_file "spirv-as" "bin" "bin"
deploy_file "spirv-dis" "bin" "bin"
deploy_file "spirv-val" "bin" "bin"

deploy_folder "clang" "${DEPLOY_TARGET}/clang" 0
deploy_folder "libcxx" "${DEPLOY_TARGET}/libcxx" 0
deploy_folder "${DEPLOY_FLOOR_INCLUDE_FOLDER}" "${DEPLOY_TARGET}/floor" ${DEPLOY_SYMLINK_FLOOR_INCLUDE}

info "done"
