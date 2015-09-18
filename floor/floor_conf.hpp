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

#ifndef __FLOOR_CONF_HPP__
#define __FLOOR_CONF_HPP__

// when building on windows, always disable these for now
#if defined(_MSC_VER)
#define FLOOR_NO_OPENAL 1
#define FLOOR_NO_NET 1
#define FLOOR_NO_XML 1
#define FLOOR_NO_EXCEPTIONS 1
#endif

// if defined, this disables cuda support
//#define FLOOR_NO_CUDA 1

// if defined, this disables host compute support
//#define FLOOR_NO_HOST_COMPUTE 1

// if defined, this disable opencl support
//#define FLOOR_NO_OPENCL 1

// if defined, this disable metal support
#if defined(__APPLE__)
//#define FLOOR_NO_METAL 1
#else
#define FLOOR_NO_METAL 1
#endif

// if defined, this disabled openal support
//#define FLOOR_NO_OPENAL 1

// if defined, this disabled network support
//#define FLOOR_NO_NET 1

// if defined, this disabled xml support
//#define FLOOR_NO_XML 1

// if defined, this disables c++ exception support (implies no-net!)
//#define FLOOR_NO_EXCEPTIONS 1
#if defined(FLOOR_NO_EXCEPTIONS) && !defined(FLOOR_NO_NET)
#error "disabled exception support also requires building without net support! (build with ./build.sh no-exceptions no-net)"
#endif

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
#endif

#endif
