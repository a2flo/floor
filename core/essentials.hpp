/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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
#if __has_include("floor/floor_conf.hpp")
#include "floor/floor_conf.hpp"
#endif

// for flagging unreachable code
#if defined(__clang__) || defined(__GNUC__)
#define floor_unreachable __builtin_unreachable
#else
#define floor_unreachable
#endif

// for flagging unused parameters/variables
#define floor_unused __attribute__((unused))

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

// we don't need these and they cause issues
#if defined(min)
#undef min
#endif
#if defined(max)
#undef max
#endif

// if any bool macros are defined for whatever insane reasons, undefine them, because they causes issues
#if defined(bool)
#undef bool
#endif
#if defined(true)
#undef true
#endif
#if defined(false)
#undef false
#endif

#endif
