/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License only.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

// floor version and build/compiler info
#include <floor/build_version.hpp>

#define FLOOR_VERSION_STRINGIFY(ver) #ver
#define FLOOR_VERSION_EVAL(ver) FLOOR_VERSION_STRINGIFY(ver)

// <major>.<minor>.<revision><dev_stage>-<build>
#define FLOOR_MAJOR_VERSION 0
#define FLOOR_MINOR_VERSION 5
#define FLOOR_REVISION_VERSION 0
#define FLOOR_DEV_STAGE_VERSION 0xa2
#define FLOOR_DEV_STAGE_VERSION_STR "a2"
// FLOOR_BUILD_VERSION defined in build_version.hpp

#define FLOOR_MAJOR_VERSION_STR FLOOR_VERSION_EVAL(FLOOR_MAJOR_VERSION)
#define FLOOR_MINOR_VERSION_STR FLOOR_VERSION_EVAL(FLOOR_MINOR_VERSION)
#define FLOOR_REVISION_VERSION_STR FLOOR_VERSION_EVAL(FLOOR_REVISION_VERSION)
#define FLOOR_BUILD_VERSION_STR FLOOR_VERSION_EVAL(FLOOR_BUILD_VERSION)

#define FLOOR_COMPAT_VERSION FLOOR_MAJOR_VERSION_STR "." FLOOR_MINOR_VERSION_STR "." FLOOR_REVISION_VERSION_STR
#define FLOOR_FULL_VERSION FLOOR_COMPAT_VERSION FLOOR_DEV_STAGE_VERSION_STR "-" FLOOR_BUILD_VERSION_STR

#define FLOOR_VERSION_U32 uint32_t((FLOOR_MAJOR_VERSION << 24u) | (FLOOR_MINOR_VERSION << 16u) | (FLOOR_REVISION_VERSION << 8u) | FLOOR_DEV_STAGE_VERSION)

#define FLOOR_BUILD_TIME __TIME__
#define FLOOR_BUILD_DATE __DATE__

#if defined(FLOOR_DEBUG) || defined(DEBUG)
#define FLOOR_DEBUG_STR " (debug)"
#else
#define FLOOR_DEBUG_STR ""
#endif

#if defined(_MSC_VER)
#define FLOOR_MSC_VERSION_STR FLOOR_VERSION_EVAL(_MSC_FULL_VER)
#define FLOOR_COMPILER "Clang " __clang_version__ " / VS " FLOOR_MSC_VERSION_STR
#elif defined(__clang__)
#define FLOOR_COMPILER "Clang " __clang_version__
#else
#define FLOOR_COMPILER "unknown compiler"
#endif

#define FLOOR_LIBCXX_PREFIX " and "
#if defined(_LIBCPP_VERSION)
#define FLOOR_LIBCXX FLOOR_LIBCXX_PREFIX "libc++ " FLOOR_VERSION_EVAL(_LIBCPP_VERSION)
#elif defined(__GLIBCXX__)
#define FLOOR_LIBCXX FLOOR_LIBCXX_PREFIX "libstdc++ " FLOOR_VERSION_EVAL(__GLIBCXX__)
#elif defined(_MSVC_STL_VERSION) && defined(_MSVC_STL_UPDATE)
#define FLOOR_LIBCXX FLOOR_LIBCXX_PREFIX "MSVC-STL " FLOOR_VERSION_EVAL(_MSVC_STL_VERSION) "-" FLOOR_VERSION_EVAL(_MSVC_STL_UPDATE)
#else
#define FLOOR_LIBCXX ""
#endif

#if defined(__x86_64__)
#define FLOOR_PLATFORM (sizeof(void*) == 8 ? "x86-64" : "unknown")
#elif defined(__aarch64__)
#define FLOOR_PLATFORM (sizeof(void*) == 8 ? "ARM64" : "unknown")
#else
#error "unhandled arch"
#endif

#define FLOOR_VERSION_STRING (std::string("floor ")+FLOOR_PLATFORM+FLOOR_DEBUG_STR \
" v"+(FLOOR_FULL_VERSION)+\
" ("+FLOOR_BUILD_DATE+" "+FLOOR_BUILD_TIME+") built with " FLOOR_COMPILER FLOOR_LIBCXX)

#define FLOOR_SOURCE_URL "https://github.com/a2flo/floor"


// compiler checks:
// msvc check
#if defined(_MSC_VER)
#if !defined(__clang__)
#error "Sorry, you need clang/LLVM and VS2022 to compile floor (http://llvm.org/builds/)"
#elif (_MSC_VER < 1930)
#error "Sorry, but you need VS2022 to compile floor"
#endif

// clang check
#elif defined(__clang__)
#if !defined(__APPLE__)
// official version
#if (__clang_major__ < 19)
#error "Sorry, but you need clang 19.0+ to compile floor"
#endif
#else
// Apple version
#if (__clang_major__ < 17)
#error "Sorry, but you need Apple clang 17.0+ (Xcode 16.3) to compile floor"
#endif
#endif

// gcc check
#elif defined(__GNUC__)
#error "Sorry, GCC is not supported"
#endif

// library checks:
#include <floor/core/platform.hpp>

#if (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 19000)
#error "You need to install libc++ 19.0+ to compile floor"
#endif

#if !SDL_VERSION_ATLEAST(3, 2, 0)
#error "You need to install SDL 3.2.0+ to compile floor"
#endif
