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

#ifndef __FLOOR_ESSENTIALS_HPP__
#define __FLOOR_ESSENTIALS_HPP__

// for clang compat
// will be part of a future std: http://isocpp.org/std/standing-documents/sd-6-sg10-feature-test-recommendations
#if !defined(__has_feature)
#define __has_feature(x) 0
#endif
#if !defined(__has_include)
#define __has_include(x) 0
#endif

// if the floor_conf.hpp header exists, include it
#if __has_include(<floor/floor/floor_conf.hpp>)
#include <floor/floor/floor_conf.hpp>
#endif

// for flagging unreachable code
#if defined(__clang__) || defined(__GNUC__)
#define floor_unreachable __builtin_unreachable
#else
#define floor_unreachable
#endif

// for flagging unused and used parameters/variables
#define floor_unused __attribute__((unused))
#define floor_used __attribute__((used))

#if defined(FLOOR_DEBUG)
#define floor_unused_if_debug __attribute__((unused))
#define floor_unused_if_release
#else
#define floor_unused_if_debug
#define floor_unused_if_release __attribute__((unused))
#endif

// for packing structs/classes (removes padding bytes)
#if defined(__clang__) || defined(__GNUC__)
#define floor_packed __attribute__((packed))
#else
#define floor_packed
#endif

// explicit switch/case fallthrough to the next case
#if defined(__clang__)
#define floor_fallthrough [[clang::fallthrough]]
#else
#define floor_fallthrough
#endif

// makes sure a function is (almost) always inlined (GCC+clang)
#if defined(__GNUC__)
#define floor_inline_always inline __attribute__((always_inline))
#else
#define floor_inline_always inline
#endif

// makes sure a function is (almost) never inlined (GCC+clang)
#if defined(__GNUC__)
#define floor_noinline __attribute__((noinline))
#else
#define floor_noinline
#endif

// marks a function as hidden / doesn't export it
#if defined(__GNUC__)
#define floor_hidden __attribute__((visibility("hidden")))
#else
#define floor_hidden
#endif

// nullability attributes/qualifiers
#if defined(__clang__)
#define floor_nonnull _Nonnull
#define floor_null_unspecified _Null_unspecified
#define floor_nullable _Nullable
#define floor_returns_nonnull __attribute__((returns_nonnull))
#else
#define floor_nonnull
#define floor_null_unspecified
#define floor_nullable
#define floor_returns_nonnull
#endif

// set compute defines if host-based compute is enabled
#if !defined(FLOOR_NO_HOST_COMPUTE) && !defined(FLOOR_COMPUTE)
#define FLOOR_COMPUTE 1
#define FLOOR_COMPUTE_HOST 1
#endif

// if host-based compute is enabled, enable host-based graphics as well
// NOTE: right now this can only be enabled for apple platforms due to constexpr requirements
#if defined(FLOOR_COMPUTE_HOST) && FLOOR_COMPUTE_HOST == 1
#if defined(__APPLE__)
#define FLOOR_GRAPHICS_HOST 1
#endif
#endif

// compat with compute device code
#if !defined(constant) && defined(FLOOR_HOST_COMPUTE)
#define constant
#endif

// ignore warning handling
#if defined(__clang__)
#define FLOOR_PUSH_WARNINGS() _Pragma("clang diagnostic push")
#define FLOOR_POP_WARNINGS() _Pragma("clang diagnostic pop")
#define FLOOR_IGNORE_WARNING_FWD(warning_str) _Pragma(#warning_str)
#define FLOOR_IGNORE_WARNING(warning) FLOOR_IGNORE_WARNING_FWD(clang diagnostic ignored "-W"#warning)
#else
#define FLOOR_PUSH_WARNINGS()
#define FLOOR_POP_WARNINGS()
#define FLOOR_IGNORE_WARNING(warning)
#endif

// clang < 3.6 doesn't have this, so just pass it through
#if !__has_builtin(__builtin_assume_aligned)
#define __builtin_assume_aligned(ptr, alignment) (ptr)
#endif
#if !__has_builtin(__builtin_assume)
#define __builtin_assume(expr)
#endif

#endif // __FLOOR_ESSENTIALS_HPP__

// -> keep these outside the header guard

// we don't need these and they cause issues
#if defined(min)
#undef min
#endif
#if defined(max)
#undef max
#endif
#if defined(__min)
#undef __min
#endif
#if defined(__max)
#undef __max
#endif

// if any bool macros get defined for whatever insane reasons, undefine them, because they cause issues
#if defined(bool)
#undef bool
#endif
#if defined(true)
#undef true
#endif
#if defined(false)
#undef false
#endif

// defined by windows headers
#if defined(ERROR)
#undef ERROR
#endif

// some os x security framework
#if defined(INTEL)
#undef INTEL
#endif
