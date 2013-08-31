/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2013 Florian Ziesche
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
#include "floor/build_version.h"
#include "core/util.hpp"
#define FLOOR_MAJOR_VERSION "0"
#define FLOOR_MINOR_VERSION "1"
#define FLOOR_REVISION_VERSION "0"
#define FLOOR_DEV_STAGE_VERSION "d1"
#define FLOOR_BUILD_TIME __TIME__
#define FLOOR_BUILD_DATE __DATE__

#if defined(FLOOR_DEBUG) || defined(DEBUG)
#define FLOOR_DEBUG_STR " (debug)"
#else
#define FLOOR_DEBUG_STR ""
#endif

#if defined(_MSC_VER)
#define FLOOR_COMPILER "VC++ "+size_t2string(_MSC_VER)
#elif (defined(__GNUC__) && !defined(__llvm__) && !defined(__clang__))
#define FLOOR_COMPILER string("GCC ")+__VERSION__
#elif (defined(__GNUC__) && defined(__llvm__) && !defined(__clang__))
#define FLOOR_COMPILER string("LLVM-GCC ")+__VERSION__
#elif defined(__clang__)
#define FLOOR_COMPILER string("Clang ")+__clang_version__
#else
#define FLOOR_COMPILER "unknown compiler"
#endif

#define FLOOR_LIBCXX_PREFIX " and "
#if defined(_LIBCPP_VERSION)
#define FLOOR_LIBCXX FLOOR_LIBCXX_PREFIX+"libc++ "+size_t2string(_LIBCPP_VERSION)
#elif defined(__GLIBCXX__)
#define FLOOR_LIBCXX FLOOR_LIBCXX_PREFIX+"libstdc++ "+size_t2string(__GLIBCXX__)
#else
#define FLOOR_LIBCXX ""
#endif

#if !defined(FLOOR_IOS)
#define FLOOR_PLATFORM (sizeof(void*) == 4 ? "x86" : (sizeof(void*) == 8 ? "x64" : "unknown"))
#else
#define FLOOR_PLATFORM (sizeof(void*) == 4 ? "ARM" : (sizeof(void*) == 8 ? "ARM64" : "unknown"))
#endif

#define FLOOR_VERSION_STRING (string("floor ")+FLOOR_PLATFORM+FLOOR_DEBUG_STR \
" v"+(FLOOR_MAJOR_VERSION)+"."+(FLOOR_MINOR_VERSION)+"."+(FLOOR_REVISION_VERSION)+(FLOOR_DEV_STAGE_VERSION)+"-"+size_t2string(FLOOR_BUILD_VERSION)+\
" ("+FLOOR_BUILD_DATE+" "+FLOOR_BUILD_TIME+") built with "+string(FLOOR_COMPILER+FLOOR_LIBCXX))

#define FLOOR_SOURCE_URL "https://github.com/a2flo/floor"


// compiler checks:
// msvc check
#if defined(_MSC_VER)
#if (_MSC_VER <= 1800)
#error "Sorry, but you need MSVC 13.0+ (VS 2014+) to compile floor"
#endif

// clang check
#elif defined(__clang__)
#if !defined(__clang_major__) || !defined(__clang_minor__) || (__clang_major__ < 3) || (__clang_major__ == 3 && __clang_minor__ < 2)
#error "Sorry, but you need Clang 3.2+ to compile floor"
#endif

// gcc check
#elif defined(__GNUC__)
#if (__GNUC__ < 4) || (__GNUC__ == 4 && __GNUC_MINOR__ < 9)
#error "Sorry, but you need GCC 4.9+ to compile floor"
#endif

// just fall through ...
#else
#endif

#endif
