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

/*! @brief platform header
 *  do/include platform specific stuff here
 */

#ifndef __FLOOR_PLATFORM_HPP__
#define __FLOOR_PLATFORM_HPP__

///////////////////////////////////////////////////////////////
// Windows
#if (defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64) || defined(__WINDOWS__) || defined(MINGW)) && !defined(CYGWIN)

#if defined(MINGW)
#include <stdlib.h>
#include <malloc.h>
#include <wchar.h>
#endif

#if defined(_MSC_VER)
#include <Windows.h>
#include <winnt.h>
#include <direct.h>
#include <io.h>

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
#define ssize_t SSIZE_T
#endif

#define setenv(var_name, var_value, x) SetEnvironmentVariable(var_name, var_value)

#define FLOOR_OS_DIR_SLASH "\\"

#if defined(MINGW)
#include <dirent.h>
#endif

// Mac OS X
#elif defined(__APPLE__)
#include <dirent.h>
#define FLOOR_OS_DIR_SLASH "/"

// everything else (Linux, *BSD, ...)
#else
#define FLOOR_OS_DIR_SLASH "/"
#include <dirent.h>

#endif // Windows


// general includes
#if defined(__APPLE__)
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_cpuinfo.h>
#include <SDL2/SDL_platform.h>
#include <SDL2/SDL_syswm.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>

#elif defined(__WINDOWS__)
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_cpuinfo.h>
#include <SDL2/SDL_platform.h>
#include <SDL2/SDL_syswm.h>

#else
#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_cpuinfo.h>
#include <SDL_platform.h>
#include <SDL_syswm.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#endif

#if !defined(FLOOR_IOS)
#define FLOOR_DEFAULT_FRAMEBUFFER 0
#else
#define FLOOR_DEFAULT_FRAMEBUFFER 1
#endif

// c++ headers
#include <floor/core/cpp_headers.hpp>

// constexpr math functions
#include <floor/constexpr/const_math.hpp>

// floor logger
#include <floor/core/logger.hpp>

// utility functions/classes/...
#include <floor/core/util.hpp>

// const_string
#include <floor/constexpr/const_string.hpp>

// clang thread safety analysis
#include <floor/threading/thread_safety.hpp>

#endif
