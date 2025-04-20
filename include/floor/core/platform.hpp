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

///////////////////////////////////////////////////////////////
// Windows
#if (defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64) || defined(__WINDOWS__) || defined(MINGW)) && !defined(CYGWIN)

#if defined(MINGW)
#define __unused__ used
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <wchar.h>
#undef __unused__
#endif

#if defined(_MSC_VER)

#undef getcwd
#define getcwd _getcwd
#endif

// defines
#if !defined(__WINDOWS__)
#define __WINDOWS__ 1
#endif

#if !defined(__func__)
#define __func__ __FUNCTION__
#endif

#if !defined(ssize_t)
#define ssize_t ptrdiff_t
#endif

#define setenv(var_name, var_value, x) SetEnvironmentVariable(var_name, var_value)

#define FLOOR_OS_DIR_SLASH "\\"

// macOS/iOS
#elif defined(__APPLE__)
#define FLOOR_OS_DIR_SLASH "/"

// everything else (Linux, *BSD, ...)
#else
#define FLOOR_OS_DIR_SLASH "/"

#endif // Windows

// fix SDL/MinGW pollution
#define SDL_cpuinfo_h_

// general includes
#include <SDL3/SDL.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_platform.h>
#if !defined(__WINDOWS__)
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#endif

// constexpr math functions
#include <floor/constexpr/const_math.hpp>

// floor logger
#include <floor/core/logger.hpp>

// const_string
#include <floor/constexpr/const_string.hpp>

// clang thread safety analysis
#include <floor/threading/thread_safety.hpp>
