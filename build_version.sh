#!/bin/sh

# determine the number of source files that have a timestamp newer than build_version
NEW_SOURCE_COUNT=$(find include src -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' -o -name '*.mm' -o -name '*.c' -o -name '*.m' \) -newer build_version | grep -v "build_version.hpp" | grep -v "floor_conf.hpp" | wc -l | tr -cd [:digit:])

BUILD_VERSION=$(cat build_version)
if [ ${NEW_SOURCE_COUNT} -gt 0 ]; then
        # if this is > 0, increase build version
        BUILD_VERSION=$(expr ${BUILD_VERSION} + 1)
        echo "${BUILD_VERSION}" > build_version
        echo "# increased build version to ${BUILD_VERSION}"
fi

# generate build_version.hpp if it's nonexistent or build version has been increased
if [ ! -f include/floor/build_version.hpp -o ${NEW_SOURCE_COUNT} -gt 0 ]; then
        echo "#define FLOOR_BUILD_VERSION ${BUILD_VERSION}" > include/floor/build_version.hpp
fi
