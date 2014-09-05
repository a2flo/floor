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

/*! @brief platform header
 *  do/include platform specific stuff here
 */

#ifndef __FLOOR_PLATFORM_HPP__
#define __FLOOR_PLATFORM_HPP__

///////////////////////////////////////////////////////////////
// Windows
#if (defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64) || defined(__WINDOWS__) || defined(MINGW)) && !defined(CYGWIN)

#include <windows.h>
#include <winnt.h>
#include <io.h>
#include <direct.h>

// defines
#if !defined(__WINDOWS__)
#define __WINDOWS__ 1
#endif

#if !defined(strtof)
#define strtof(arg1, arg2) ((float)strtod(arg1, arg2))
#endif

#if !defined(__func__)
#define __func__ __FUNCTION__
#endif

#if !defined(__FLT_MAX__)
#define __FLT_MAX__ FLT_MAX
#endif

#if !defined(SIZE_T_MAX)
#if defined(MAXSIZE_T)
#define SIZE_T_MAX MAXSIZE_T
#else
#define SIZE_T_MAX (~((size_t)0))
#endif
#endif

#if !defined(ssize_t)
#define ssize_t SSIZE_T
#endif

#undef getcwd
#define getcwd _getcwd

#define setenv(var_name, var_value, x) SetEnvironmentVariable(var_name, var_value)

#pragma warning(disable: 4251)
#pragma warning(disable: 4290) // unnecessary exception throw warning
#pragma warning(disable: 4503) // srsly microsoft? this ain't the '80s ...

#define FLOOR_OS_DIR_SLASH "\\"

// Mac OS X
#elif defined(__APPLE__)
#include <dirent.h>
#define FLOOR_API
#define FLOOR_OS_DIR_SLASH "/"

// everything else (Linux, *BSD, ...)
#else
#define FLOOR_API
#define FLOOR_OS_DIR_SLASH "/"
#include <dirent.h>

#if !defined(SIZE_T_MAX)
#define SIZE_T_MAX (~((size_t)0))
#endif

#endif // Windows


// general includes
#if defined(__APPLE__)
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_cpuinfo.h>
#include <SDL2/SDL_platform.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2_image/SDL_image.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>

#elif defined(__WINDOWS__) && !defined(WIN_UNIXENV)
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_cpuinfo.h>
#include <SDL2/SDL_platform.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_image.h>

#elif defined(MINGW)
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_cpuinfo.h>
#include <SDL2/SDL_platform.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_image.h>

#else
#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_cpuinfo.h>
#include <SDL_image.h>
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

#if !defined(__has_feature)
#define __has_feature(x) 0
#endif

#if defined(__clang__) || defined(__GNUC__)
#define floor_unreachable __builtin_unreachable
#else
#define floor_unreachable
#endif

#define floor_unused __attribute__((unused))

#if defined(__clang__) || defined(__GNUC__)
#define floor_packed __attribute__((packed))
#else
#define floor_packed
#endif

#if defined(__clang__)
#define floor_fallthrough [[clang::fallthrough]]
#else
#define floor_fallthrough
#endif

#endif
