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

// NOTE: this may be included more than once (see below)
#ifndef __FLOOR_ESSENTIALS_HPP__
#define __FLOOR_ESSENTIALS_HPP__

// if the floor_conf.hpp header exists, include it
#if __has_include(<floor/floor_conf.hpp>)
#include <floor/floor_conf.hpp>
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
#define floor_force_nonnull(value) ([](auto param_) { return param_; }(value))
#else
#define floor_nonnull
#define floor_null_unspecified
#define floor_nullable
#define floor_returns_nonnull
#define floor_force_nonnull(value) (value)
#endif

// set compute defines if host-based compute is enabled
#if !defined(FLOOR_NO_HOST_COMPUTE) && !defined(FLOOR_DEVICE)
#define FLOOR_DEVICE 1
#define FLOOR_DEVICE_HOST_COMPUTE 1
#endif

// if Host-Compute is enabled, enable host-based graphics as well
// NOTE: right now this can only be enabled for apple platforms due to constexpr requirements
#if defined(FLOOR_DEVICE_HOST_COMPUTE) && FLOOR_DEVICE_HOST_COMPUTE == 1
#if defined(__APPLE__)
#define FLOOR_GRAPHICS_HOST_COMPUTE 1
#endif
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

// on Windows: even with auto-export of all symbols, we still need to manually export global variables -> add API macros
#if defined(__WINDOWS__) && !defined(MINGW)
#if defined(FLOOR_DLL_EXPORT)
#pragma warning(disable: 4251)
#define FLOOR_DLL_API __declspec(dllexport)
#else
#pragma warning(disable: 4251)
#define FLOOR_DLL_API __declspec(dllimport)
#endif
#else
#define FLOOR_DLL_API /* nop */
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
#if defined(RELATIVE)
#undef RELATIVE
#endif

// some macOS security framework
#if defined(INTEL)
#undef INTEL
#endif

// MSVC workarounds
#if defined(_MSC_VER)

#if !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif

#endif // _MSC_VER

#if defined(MINGW)
#define __unused__ used
#include <cstdio>
#undef __unused__
#endif
