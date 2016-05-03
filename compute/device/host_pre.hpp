/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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
// dllexport (windows only): necessary, so that the function can be retrieved via GetProcAddress
#if !defined(__WINDOWS__)
#define kernel extern "C" inline __attribute__((used, visibility("default")))
#else
#define kernel extern "C" inline __attribute__((used, visibility("default"))) __declspec(dllexport)
#endif

// workaround use of "global" in locale header by including it before killing global
#include <locale>
// provide alternate function
floor_inline_always static std::locale locale_global(const std::locale& loc) {
	return std::locale::global(loc);
}

// kill address space keywords
#define global
#define local
#define constant

// sized integer types
#include <cstdint>

// these would usually be set through llvm_compute at compile-time

// toolchain version is just (MAJOR * 100 + MINOR * 10 + PATCHLEVEL), e.g. 352 for clang v3.5.2
#if !defined(__apple_build_version__)
#define FLOOR_COMPUTE_TOOLCHAIN_VERSION (__clang_major__ * 100u + __clang_minor__ * 10u + __clang_patchlevel__)
#else // map apple version scheme ... (*sigh*)
#if (__clang_major__ < 6) || (__clang_major__ == 6 && __clang_minor__ < 1) // Xcode 6.3 with clang 3.6.0 is the min req.
#error "unsupported toolchain"
#endif

#if (__clang_major__ == 6)
#define FLOOR_COMPUTE_TOOLCHAIN_VERSION 360u
#elif (__clang_major__ == 7 && __clang_minor__ < 3)
#define FLOOR_COMPUTE_TOOLCHAIN_VERSION 370u
#else // Xcode 7.3.0+
#define FLOOR_COMPUTE_TOOLCHAIN_VERSION 380u
#endif

#endif

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

// use compiler specific define to detect if this is wanted or not
#if defined(__FMA__)
#define FLOOR_COMPUTE_INFO_HAS_FMA 1
#define FLOOR_COMPUTE_INFO_HAS_FMA_1
#else
#define FLOOR_COMPUTE_INFO_HAS_FMA 0
#define FLOOR_COMPUTE_INFO_HAS_FMA_0
#endif

// these are always set, as all targets (x86/arm) should/must support these
#define FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS 1
#define FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1
#define FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS 1
#define FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1

// local memory is emulated through global ("normal") memory, although almost certainly cached
#define FLOOR_COMPUTE_INFO_HAS_DEDICATED_LOCAL_MEMORY 0
#define FLOOR_COMPUTE_INFO_HAS_DEDICATED_LOCAL_MEMORY_0

// handle simd-width, as this obviously needs to be known at compile-time (even though it might be different at run-time),
// make this dependent on compiler specific defines
#if !defined(FLOOR_COMPUTE_INFO_SIMD_WIDTH_OVERRIDE) && !defined(FLOOR_COMPUTE_INFO_SIMD_WIDTH) // use these to override
#if defined(__AVX512F__)
#define FLOOR_COMPUTE_INFO_SIMD_WIDTH 16u
#define FLOOR_COMPUTE_INFO_SIMD_WIDTH_16
#elif defined(__AVX__)
#define FLOOR_COMPUTE_INFO_SIMD_WIDTH 8u
#define FLOOR_COMPUTE_INFO_SIMD_WIDTH_8
#else // fallback to always 4 (sse/neon)
#define FLOOR_COMPUTE_INFO_SIMD_WIDTH 4u
#define FLOOR_COMPUTE_INFO_SIMD_WIDTH_4
#endif
#endif
#define FLOOR_COMPUTE_INFO_SIMD_WIDTH_MIN 1u
#define FLOOR_COMPUTE_INFO_SIMD_WIDTH_MAX FLOOR_COMPUTE_INFO_SIMD_WIDTH

// image info
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_SUPPORT_1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_SUPPORT_1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_WRITE_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_WRITE_SUPPORT_1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_SUPPORT 0
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_SUPPORT_0
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_WRITE_SUPPORT 0
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_WRITE_SUPPORT_0
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_SUPPORT_1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_WRITE_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_WRITE_SUPPORT_1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_SUPPORT 0
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_SUPPORT_0
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_WRITE_SUPPORT 0
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_WRITE_SUPPORT_0
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_READ_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_READ_SUPPORT_1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_WRITE_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_WRITE_SUPPORT_1

#define FLOOR_COMPUTE_INFO_MAX_MIP_LEVELS 16u

// for use with "#pragma clang loop unroll(x)", this is named "full" after clang 3.5, and "enable" for 3.5
#if (__clang_major__ == 3 && __clang_minor__ == 5)
#define FLOOR_CLANG_UNROLL_FULL enable
#else
#define FLOOR_CLANG_UNROLL_FULL full
#endif

// other required c++ headers
#include <vector>
#include <limits>
#include <string>

#endif

#endif
