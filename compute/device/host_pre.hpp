/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

// kill address space keywords
#if defined(FLOOR_COMPUTE_HOST_DEVICE)
#define global
#define local
#define constant
#endif

// sized integer types
#include <stdint.h>
#include <stddef.h>
#include <cstdint>
#if defined(FLOOR_COMPUTE_HOST_DEVICE)
typedef __SIZE_TYPE__ size_t;
#endif

// used to mark kernel functions which must be dynamically retrievable at runtime
// extern "C": use C name mangling instead of C++ mangling (so function name is the same as written in the code)
// inline: not actually inline, but makes sure that no prototype is required for global functions
// attribute used: emit the function even if it apparently seems unused
// visibility default: function name is publicly visible and can be retrieved at runtime
// dllexport (windows only): necessary, so that the function can be retrieved via GetProcAddress
#if defined(FLOOR_COMPUTE_HOST_DEVICE) // device toolchain
#define kernel extern "C" __attribute__((compute_kernel, used, visibility("default")))
#define vertex extern "C" __attribute__((vertex_shader, used, visibility("default")))
#define fragment extern "C" __attribute__((fragment_shader, used, visibility("default")))
#else // host toolchain
#if !defined(__WINDOWS__)
#define FLOOR_ENTRY_POINT_SPEC inline __attribute__((used, visibility("default")))
#else
#define FLOOR_ENTRY_POINT_SPEC inline __attribute__((used, visibility("default"))) __declspec(dllexport)
#endif
#define FLOOR_ENTRY_POINT_SPEC_C extern "C" FLOOR_ENTRY_POINT_SPEC

// NOTE: kernel always returns void -> can use extern "C"
//       shaders can return complex return types -> must use C++ mangling
#define kernel FLOOR_ENTRY_POINT_SPEC_C
#define vertex FLOOR_ENTRY_POINT_SPEC
#define fragment FLOOR_ENTRY_POINT_SPEC
#endif

// define calling convention
#if defined(__x86_64__)
#define FLOOR_COMPUTE_HOST_CALLING_CONV sysv_abi
#elif defined(__aarch64__)
#define FLOOR_COMPUTE_HOST_CALLING_CONV pcs("aapcs")
#else
#error "unhandled arch"
#endif

// workaround use of "global" in locale header by including it before killing global
#if !defined(FLOOR_COMPUTE_HOST_DEVICE)
#include <locale>
// provide alternate function
floor_inline_always static std::locale locale_global(const std::locale& loc) {
	return std::locale::global(loc);
}
#endif

// kill address space keywords
#if !defined(FLOOR_COMPUTE_HOST_DEVICE)
#define global
#define local
#define constant
#endif

// limits
#include <floor/compute/device/host_limits.hpp>

// these would usually be set through llvm_toolchain at compile-time

// toolchain version is just (MAJOR * 10000 + MINOR * 100 + PATCHLEVEL), e.g. 30502 for clang v3.5.2
#if !defined(__apple_build_version__)
#define FLOOR_TOOLCHAIN_VERSION (__clang_major__ * 10000u + __clang_minor__ * 100u + __clang_patchlevel__)
#else // map apple version scheme ... (*sigh*)
#if (__clang_major__ == 12 && __clang_minor__ == 0 && __clang_patchlevel__ < 5) // Xcode 12.5 with clang 11.0 is the min req.
#error "unsupported toolchain"
#endif

#if __clang_major__ == 12 // Xcode 12.5+
#define FLOOR_TOOLCHAIN_VERSION 110000u
#elif (__clang_major__ == 13 && __clang_minor__ == 0) // Xcode 13.0 - 13.2
#define FLOOR_TOOLCHAIN_VERSION 120000u
#elif (__clang_major__ == 13 && __clang_minor__ >= 1) // Xcode 13.3
#define FLOOR_TOOLCHAIN_VERSION 130000u
#else // newer/unreleased Xcode, default to 13.0 for now
#define FLOOR_TOOLCHAIN_VERSION 130000u
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
#if FLOOR_COMPUTE_INFO_OS_VERSION < 101300
#error "invalid os version"
#elif FLOOR_COMPUTE_INFO_OS_VERSION >= 101300 && FLOOR_COMPUTE_INFO_OS_VERSION < 101400
#define FLOOR_COMPUTE_INFO_OS_VERSION_101300
#elif FLOOR_COMPUTE_INFO_OS_VERSION >= 101400 && FLOOR_COMPUTE_INFO_OS_VERSION < 101500
#define FLOOR_COMPUTE_INFO_OS_VERSION_101400
#elif FLOOR_COMPUTE_INFO_OS_VERSION >= 101500 && FLOOR_COMPUTE_INFO_OS_VERSION < 110000
#define FLOOR_COMPUTE_INFO_OS_VERSION_101500
#elif FLOOR_COMPUTE_INFO_OS_VERSION >= 110000 && FLOOR_COMPUTE_INFO_OS_VERSION < 120000
#define FLOOR_COMPUTE_INFO_OS_VERSION_110000
#elif FLOOR_COMPUTE_INFO_OS_VERSION >= 120000
#define FLOOR_COMPUTE_INFO_OS_VERSION_120000
#endif

#elif defined(__APPLE__) && defined(FLOOR_IOS) // ios
#define FLOOR_COMPUTE_INFO_OS_VERSION __IPHONE_OS_VERSION_MAX_ALLOWED
#if FLOOR_COMPUTE_INFO_OS_VERSION < 120000
#error "invalid os version"
#elif FLOOR_COMPUTE_INFO_OS_VERSION >= 120000 && FLOOR_COMPUTE_INFO_OS_VERSION < 130000
#define FLOOR_COMPUTE_INFO_OS_VERSION_120000
#elif FLOOR_COMPUTE_INFO_OS_VERSION >= 130000 && FLOOR_COMPUTE_INFO_OS_VERSION < 140000
#define FLOOR_COMPUTE_INFO_OS_VERSION_130000
#elif FLOOR_COMPUTE_INFO_OS_VERSION >= 140000 && FLOOR_COMPUTE_INFO_OS_VERSION < 150000
#define FLOOR_COMPUTE_INFO_OS_VERSION_140000
#elif FLOOR_COMPUTE_INFO_OS_VERSION >= 150000
#define FLOOR_COMPUTE_INFO_OS_VERSION_150000
#endif

#else // all else
#define FLOOR_COMPUTE_INFO_OS_VERSION 0
#define FLOOR_COMPUTE_INFO_OS_VERSION_0
#endif

// always disable this (no native fma functions should be used),
// this way the optimizer and vectorizer can actually generate proper code
#define FLOOR_COMPUTE_INFO_HAS_FMA 0
#define FLOOR_COMPUTE_INFO_HAS_FMA_0

// these are always set, as all targets (x86/arm) should/must support these
#define FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS 1
#define FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1
#define FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS 1
#define FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1
// not natively supported or exposed right now
#define FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS 0
#define FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_0

// local memory is emulated through global ("normal") memory, although almost certainly cached
#define FLOOR_COMPUTE_INFO_HAS_DEDICATED_LOCAL_MEMORY 0
#define FLOOR_COMPUTE_INFO_HAS_DEDICATED_LOCAL_MEMORY_0

// host-compute doesn't support sub-groups or sub-group shuffle right now
#define FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS 0
#define FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS_0
#define FLOOR_COMPUTE_INFO_HAS_SUB_GROUP_SHUFFLE 0
#define FLOOR_COMPUTE_INFO_HAS_SUB_GROUP_SHUFFLE_0

// host-compute doesn't support cooperative kernels right now
#define FLOOR_COMPUTE_INFO_HAS_COOPERATIVE_KERNEL 0
#define FLOOR_COMPUTE_INFO_HAS_COOPERATIVE_KERNEL_0

// host-compute doesn't support primitive ID or barycentric coordinates
#define FLOOR_COMPUTE_INFO_HAS_PRIMITIVE_ID 0
#define FLOOR_COMPUTE_INFO_HAS_PRIMITIVE_ID_0
#define FLOOR_COMPUTE_INFO_HAS_BARYCENTRIC_COORD 0
#define FLOOR_COMPUTE_INFO_HAS_BARYCENTRIC_COORD_0

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
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_ARRAY_SUPPORT 0
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_ARRAY_SUPPORT_0
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_ARRAY_WRITE_SUPPORT 0
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_ARRAY_WRITE_SUPPORT_0
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_SUPPORT_1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_WRITE_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_WRITE_SUPPORT_1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_ARRAY_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_ARRAY_SUPPORT_1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_ARRAY_WRITE_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_ARRAY_WRITE_SUPPORT_1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_SUPPORT_1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_WRITE_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_WRITE_SUPPORT_1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_READ_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_READ_SUPPORT_1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_WRITE_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_WRITE_SUPPORT_1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_COMPARE_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_COMPARE_SUPPORT_1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_GATHER_SUPPORT 0
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_GATHER_SUPPORT_0
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_READ_WRITE_SUPPORT 1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_READ_WRITE_SUPPORT_1

#define FLOOR_COMPUTE_INFO_MAX_MIP_LEVELS 16u

// indirect command info
#define FLOOR_COMPUTE_INFO_INDIRECT_COMMAND_SUPPORT 0
#define FLOOR_COMPUTE_INFO_INDIRECT_COMMAND_SUPPORT_0
#define FLOOR_COMPUTE_INFO_INDIRECT_COMPUTE_COMMAND_SUPPORT 0
#define FLOOR_COMPUTE_INFO_INDIRECT_COMPUTE_COMMAND_SUPPORT_0
#define FLOOR_COMPUTE_INFO_INDIRECT_RENDER_COMMAND_SUPPORT 0
#define FLOOR_COMPUTE_INFO_INDIRECT_RENDER_COMMAND_SUPPORT_0

// other required c++ headers
#if !defined(FLOOR_COMPUTE_HOST_DEVICE)
#include <vector>
#include <string>
#endif
#include <limits>

#endif

#endif
