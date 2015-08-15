/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_HOST_PRE_HPP__
#define __FLOOR_COMPUTE_DEVICE_HOST_PRE_HPP__

#if defined(FLOOR_COMPUTE_HOST)

#if defined(__APPLE__)
#include <AvailabilityMacros.h>
#endif

// used to mark kernel functions which must be dynamically retrievable at runtime
// extern "C": use C name mangling instead of C++ mangling (so function name is the same as written in the code)
// inline: not actually inline, but makes sure that no prototype is required for global functions
// attribute used: emit the function even if it apparently seems unused
// visibility default: function name is publicly visible and can be retrieved at runtime
#define kernel extern "C" inline __attribute__((used, visibility("default")))

// workaround use of "global" in locale header by including it before killing global
#include <locale>

// kill address space keywords
#define global
#define local
#define constant

// sized integer types
#include <cstdint>

// these would usually be set through llvm_compute at compile-time
#define FLOOR_COMPUTE_INFO_VENDOR HOST
#define FLOOR_COMPUTE_INFO_VENDOR_HOST
#define FLOOR_COMPUTE_INFO_PLATFORM_VENDOR HOST
#define FLOOR_COMPUTE_INFO_PLATFORM_VENDOR_HOST
#define FLOOR_COMPUTE_INFO_TYPE CPU
#define FLOOR_COMPUTE_INFO_TYPE_CPU

#if defined(__APPLE__)
#if defined(FLOOR_IOS)
#define FLOOR_COMPUTE_INFO_OS IOS
#define FLOOR_COMPUTE_INFO_OS_IOS
#else
#define FLOOR_COMPUTE_INFO_OS OSX
#define FLOOR_COMPUTE_INFO_OS_OSX
#endif
#elif defined(__WINDOWS__)
#define FLOOR_COMPUTE_INFO_OS WINDOWS
#define FLOOR_COMPUTE_INFO_OS_WINDOWS
#elif defined(__LINUX__)
#define FLOOR_COMPUTE_INFO_OS LINUX
#define FLOOR_COMPUTE_INFO_OS_LINUX
#elif defined(__FreeBSD__)
#define FLOOR_COMPUTE_INFO_OS FREEBSD
#define FLOOR_COMPUTE_INFO_OS_FREEBSD
#elif defined(__OpenBSD__)
#define FLOOR_COMPUTE_INFO_OS OPENBSD
#define FLOOR_COMPUTE_INFO_OS_OPENBSD
#else
#define FLOOR_COMPUTE_INFO_OS UNKNOWN
#define FLOOR_COMPUTE_INFO_OS_UNKNOWN
#endif

#if defined(__APPLE__) && !defined(FLOOR_IOS) // osx
#define FLOOR_COMPUTE_INFO_OS_VERSION MAC_OS_X_VERSION_MAX_ALLOWED
#if FLOOR_COMPUTE_INFO_OS_VERSION < 1090
#error "invalid os version"
#elif FLOOR_COMPUTE_INFO_OS_VERSION >= 1090 && FLOOR_COMPUTE_INFO_OS_VERSION < 101000
#define FLOOR_COMPUTE_INFO_OS_VERSION_1090
#elif FLOOR_COMPUTE_INFO_OS_VERSION >= 101000 && FLOOR_COMPUTE_INFO_OS_VERSION < 101100
#define FLOOR_COMPUTE_INFO_OS_VERSION_101000
#elif FLOOR_COMPUTE_INFO_OS_VERSION >= 101100
#define FLOOR_COMPUTE_INFO_OS_VERSION_101100
#endif

#elif defined(__APPLE__) && defined(FLOOR_IOS) // ios
#define FLOOR_COMPUTE_INFO_OS_VERSION __IPHONE_OS_VERSION_MAX_ALLOWED
#if FLOOR_COMPUTE_INFO_OS_VERSION < 70000
#error "invalid os version"
#elif FLOOR_COMPUTE_INFO_OS_VERSION >= 70000 && FLOOR_COMPUTE_INFO_OS_VERSION < 80000
#define FLOOR_COMPUTE_INFO_OS_VERSION_70000
#elif FLOOR_COMPUTE_INFO_OS_VERSION >= 80000 && FLOOR_COMPUTE_INFO_OS_VERSION < 90000
#define FLOOR_COMPUTE_INFO_OS_VERSION_80000
#elif FLOOR_COMPUTE_INFO_OS_VERSION >= 90000
#define FLOOR_COMPUTE_INFO_OS_VERSION_90000
#endif

#else // all else
#define FLOOR_COMPUTE_INFO_OS_VERSION 0
#define FLOOR_COMPUTE_INFO_OS_VERSION_0
#endif

// always disabled for now, can't properly detect this at compile-time or handle backwards-compat
#define FLOOR_COMPUTE_INFO_HAS_FMA 0
#define FLOOR_COMPUTE_INFO_HAS_FMA_0

// these are always set, as all targets (x86/arm) should/must support these
#define FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS 1
#define FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1
#define FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS 1
#define FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1

// other required c++ headers
#include <vector>
#include <limits>
#include <string>

#endif

#endif
