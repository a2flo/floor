/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#ifndef __FLOOR_VERSION_HPP__
#define __FLOOR_VERSION_HPP__

// floor version and build/compiler info
#include <floor/floor/build_version.hpp>
#include <floor/core/util.hpp>

#define FLOOR_VERSION_STRINGIFY(ver) #ver
#define FLOOR_VERSION_EVAL(ver) FLOOR_VERSION_STRINGIFY(ver)

// <major>.<minor>.<revision><dev_stage>-<build>
#define FLOOR_MAJOR_VERSION 0
#define FLOOR_MINOR_VERSION 3
#define FLOOR_REVISION_VERSION 0
#define FLOOR_DEV_STAGE_VERSION 0xa9
#define FLOOR_DEV_STAGE_VERSION_STR "a9"
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
#if defined(__clang__)
#define FLOOR_COMPILER "Clang " __clang_version__ " / VS " _MSC_VER
#else
#define FLOOR_COMPILER "VC++ " _MSC_VER
#endif
#elif (defined(__GNUC__) && !defined(__llvm__) && !defined(__clang__))
#define FLOOR_COMPILER "GCC " __VERSION__
#elif (defined(__GNUC__) && defined(__llvm__) && !defined(__clang__))
#define FLOOR_COMPILER "LLVM-GCC " __VERSION__
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
#else
#define FLOOR_LIBCXX ""
#endif

#if !defined(FLOOR_IOS)
#define FLOOR_PLATFORM (sizeof(void*) == 4 ? "x86" : (sizeof(void*) == 8 ? "x64" : "unknown"))
#else
#define FLOOR_PLATFORM (sizeof(void*) == 4 ? "ARM32" : (sizeof(void*) == 8 ? "ARM64" : "unknown"))
#endif

#define FLOOR_VERSION_STRING (string("floor ")+FLOOR_PLATFORM+FLOOR_DEBUG_STR \
" v"+(FLOOR_FULL_VERSION)+\
" ("+FLOOR_BUILD_DATE+" "+FLOOR_BUILD_TIME+") built with " FLOOR_COMPILER FLOOR_LIBCXX)

#define FLOOR_SOURCE_URL "https://github.com/a2flo/floor"


// compiler checks:
// msvc check
#if defined(_MSC_VER)
#if !defined(__clang__)
#error "Sorry, you need clang/llvm and VS2015 to compile floor (http://llvm.org/builds/)"
#elif (_MSC_VER < 1900)
#error "Sorry, but you need VS2015 to compile floor"
#endif

// clang check
#elif defined(__clang__)
#if !defined(__clang_major__) || !defined(__clang_minor__) || (__clang_major__ < 4) || (__clang_major__ == 4 && __clang_minor__ < 0)
#error "Sorry, but you need Clang 4.0+ to compile floor"
#endif

// gcc check
#elif defined(__GNUC__)
#error "Sorry, GCC is not supported"
#endif

// library checks:
#include <floor/core/platform.hpp>

#if (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 6000)
#error "You need to install libc++ 6.0+ to compile floor"
#endif

#if !SDL_VERSION_ATLEAST(2, 0, 2)
#error "You need to install SDL 2.0.2+ to compile floor"
#endif

#if !defined(FLOOR_NO_NET)
#include <openssl/opensslv.h>
#if (OPENSSL_VERSION_NUMBER < 0x1000105fL)
#error "You need to install OpenSSL 1.0.1e+ to compile floor"
#endif
#endif

#endif
