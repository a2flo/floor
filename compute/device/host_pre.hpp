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

//
#define kernel extern "C"

// kill address space keywords
#define global
#define local
#define constant

// sized integer types
#include <cstdint>

// these would usually be set through llvm_compute at compile-time
// TODO: proper values
#define FLOOR_COMPUTE_INFO_VENDOR HOST
#define FLOOR_COMPUTE_INFO_VENDOR_HOST
#define FLOOR_COMPUTE_INFO_TYPE CPU
#define FLOOR_COMPUTE_INFO_TYPE_CPU
#define FLOOR_COMPUTE_INFO_OS UNKNOWN
#define FLOOR_COMPUTE_INFO_OS_UNKNOWN
#define FLOOR_COMPUTE_INFO_OS_VERSION 0
#define FLOOR_COMPUTE_INFO_OS_VERSION_0
#define FLOOR_COMPUTE_INFO_HAS_FMA 0
#define FLOOR_COMPUTE_INFO_HAS_FMA_0
#define FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS 1
#define FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1
#define FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS 1
#define FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1

#endif

#endif
