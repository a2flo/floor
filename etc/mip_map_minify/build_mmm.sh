#!/bin/sh

OCC=
if [[ -n $(command -v occ) ]]; then
	OCC=occ
elif [[ -n $(command -v /opt/floor/toolchain/bin/occ) ]]; then
	OCC=/opt/floor/toolchain/bin/occ
elif [[ -n $(command -v occd) ]]; then
	OCC=occd
elif [[ -n $(command -v /opt/floor/toolchain/bin/occd) ]]; then
	OCC=/opt/floor/toolchain/bin/occd
else
	echo "failed to find the libfloor offline compiler binary"
	exit -1
fi

# build mip map minify FUBAR
rm mmm.fubar 2>/dev/null
${OCC} --src ../../compute/device/mip_map_minify.hpp --fubar targets_mmm.json --fubar-compress --out mmm.fubar --no-double -vv -- -fdiscard-value-names
if [ ! -f mmm.fubar ]; then
	echo "failed to build mmm.fubar"
	exit -3
fi
