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

// when building on macOS/iOS, always disable opencl
// for Vulkan, put the disable behind a big ol' hack, so that we can at least get syntax
// highlighting/completion when Vulkan headers are installed (will build, but won't link though)
#if defined(__APPLE__)
#if !__has_include(<floor/vulkan_testing.hpp>) || defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
#define FLOOR_NO_VULKAN 1
#else
#define FLOOR_VULKAN_TESTING 1
#include <floor/vulkan_testing.hpp>
#endif
#define FLOOR_NO_OPENCL 1
#endif

// if defined, this disables CUDA support
//#define FLOOR_NO_CUDA 1

// if defined, this disables Host-Compute support
//#define FLOOR_NO_HOST_COMPUTE 1

// if defined, this disables OpenCL support
//#define FLOOR_NO_OPENCL 1

#if !defined(FLOOR_VULKAN_TESTING)
// if defined, this disables Vulkan support
//#define FLOOR_NO_VULKAN 1
#endif

// if defined, this disables Metal support
#if defined(__APPLE__)
//#define FLOOR_NO_METAL 1
#else
#define FLOOR_NO_METAL 1
#endif

// if defined, this disables OpenVR support
//#define FLOOR_NO_OPENVR 1

// if defined, this disables OpenXR support
//#define FLOOR_NO_OPENXR 1

// if defined, this will use extern templates for specific template classes (vector*, matrix, etc.)
// and instantiate them for various basic types (float, int, ...)
// NOTE: don't enable this for compute (these won't compile the necessary .cpp files)
#if !defined(FLOOR_EXPORT) && (!defined(FLOOR_DEVICE) || (defined(FLOOR_DEVICE_HOST_COMPUTE) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)))
#define FLOOR_EXPORT 1
#endif

// no VR support on iOS/macOS
#if defined(__APPLE__)
#if !defined(FLOOR_NO_OPENVR)
#define FLOOR_NO_OPENVR 1
#endif
#if !defined(FLOOR_NO_OPENXR)
#define FLOOR_NO_OPENXR 1
#endif
#endif
