/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2013 Florian Ziesche
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

#ifndef __FLOOR_CPP_HEADERS_HPP__
#define __FLOOR_CPP_HEADERS_HPP__

// on windows exports/imports: apparently these have to be treated separately,
// always use dllexport for floor/c++ stuff and depending on compiling or using
// floor, use dllexport or dllimport for all opengl functions
#if defined(FLOOR_EXPORTS)
#pragma warning(disable: 4251)
#define FLOOR_API __declspec(dllexport)
#define OGL_API __declspec(dllexport)
#elif defined(FLOOR_IMPORTS)
#pragma warning(disable: 4251)
#define FLOOR_API __declspec(dllexport)
#define OGL_API __declspec(dllimport)
#else
#define FLOOR_API
#define OGL_API
#endif // FLOOR_API_EXPORT

#if defined(__WINDOWS__) || defined(MINGW)
#include <windows.h>
#endif

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <functional>
#include <vector>
#include <array>
#include <list>
#include <deque>
#include <queue>
#include <stack>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <limits>
#include <typeinfo>
#include <locale>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <atomic>
#include <ctime>
#include <cstring>
#include <cmath>
#include <cassert>

#if defined(CYGWIN)
#include <sys/wait.h>
#endif

using namespace std;

//
#define floor_unused __attribute__((unused))

// we don't need these
#undef min
#undef max

// cbegin/cend
#if !defined(FLOOR_HAS_CBEGIN_CEND) && \
	/* now part of c++14 and implemented in libc++ */ \
	(!defined(_LIBCPP_STD_VER) || !(_LIBCPP_STD_VER > 11))

template <class C> auto cbegin(C& c) -> decltype(c.cbegin()) { return c.cbegin(); }
template <class C> auto cbegin(const C& c) -> decltype(c.cbegin()) { return c.cbegin(); }
template <class C> auto cend(C& c) -> decltype(c.cend()) { return c.cend(); }
template <class C> auto cend(const C& c) -> decltype(c.cend()) { return c.cend(); }
template <class T, size_t N> const T* cbegin(const T (&array)[N]) { return array; }
template <class T, size_t N> const T* cend(const T (&array)[N]) { return array + N; }

#endif

// make_unique
#if !defined(FLOOR_HAS_MAKE_UNIQUE) && \
	/* now part of c++14 and implemented in libc++ */ \
	(!defined(_LIBCPP_STD_VER) || !(_LIBCPP_STD_VER > 11))

template<typename T, typename... Args> unique_ptr<T> make_unique(Args&&... args) {
	return unique_ptr<T>(new T(std::forward<Args>(args)...));
}

#endif

#endif
