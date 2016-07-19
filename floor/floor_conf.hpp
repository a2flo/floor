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

#ifndef __FLOOR_CONF_HPP__
#define __FLOOR_CONF_HPP__

// when building on windows, always disable these for now
#if defined(_MSC_VER)
#define FLOOR_NO_VULKAN 1
#define FLOOR_NO_OPENAL 1
#define FLOOR_NO_XML 1
#define FLOOR_NO_EXCEPTIONS 1
#if !defined(FLOOR_MSVC_NET) // optional, must be enabled explicitly
#define FLOOR_NO_NET 1
#endif
#endif

// when building on osx/ios, always disable vulkan for now
#if defined(__APPLE__)
#define FLOOR_NO_VULKAN 1
#endif

// if defined, this disables cuda support
//#define FLOOR_NO_CUDA 1

// if defined, this disables host compute support
//#define FLOOR_NO_HOST_COMPUTE 1

// if defined, this disables opencl support
//#define FLOOR_NO_OPENCL 1

// if defined, this disables vulkan support
#define FLOOR_NO_VULKAN 1

// if defined, this disables metal support
#if defined(__APPLE__)
//#define FLOOR_NO_METAL 1
#else
#define FLOOR_NO_METAL 1
#endif

// if defined, this disables openal support
//#define FLOOR_NO_OPENAL 1

// if defined, this disables network support
//#define FLOOR_NO_NET 1

// if defined, this disables xml support
//#define FLOOR_NO_XML 1

// if defined, this disables c++ exception support
//#define FLOOR_NO_EXCEPTIONS 1

// if defined, this enables c++17 support
//#define FLOOR_CXX17 1

// if defined, this will use extern templates for specific template classes (vector*, matrix, etc.)
// and instantiate them for various basic types (float, int, ...)
// NOTE: don't enable this for compute (these won't compile the necessary .cpp files)
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
#define FLOOR_EXPORT 1
#endif

// use asio standalone and header-only
#if !defined(FLOOR_NO_NET)
#define ASIO_STANDALONE 1
#define ASIO_HEADER_ONLY 1
#define ASIO_NO_EXCEPTIONS 1
#define ASIO_DISABLE_BOOST_THROW_EXCEPTION 1
#define ASIO_DISABLE_BUFFER_DEBUGGING 1
#endif

#endif
