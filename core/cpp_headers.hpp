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

#ifndef __FLOOR_CPP_HEADERS_HPP__
#define __FLOOR_CPP_HEADERS_HPP__

// on windows exports/imports: apparently these have to be treated separately,
// use dllexport or dllimport for all opengl functions, depending on compiling
// floor itself or other projects using/including floor
#if defined(FLOOR_EXPORTS)
#pragma warning(disable: 4251)
#define OGL_API __declspec(dllexport)
#elif defined(FLOOR_IMPORTS)
#pragma warning(disable: 4251)
#define OGL_API __declspec(dllimport)
#else
#define OGL_API
#endif // FLOOR_EXPORTS

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
#include <chrono>
#include <ctime>
#include <cstring>
#include <cmath>
#include <cassert>
#include <stdbool.h>

#if defined(CYGWIN)
#include <sys/wait.h>
#endif

using namespace std;

// essentials (should be included after system/c++ headers)
#include <floor/core/essentials.hpp>

// minor type_trait extension
#if !defined(FLOOR_HAS_IS_VECTOR)
template <typename any_type> struct is_vector : public false_type {};
template <typename... vec_params> struct is_vector<vector<vec_params...>> : public true_type {};
#endif

// non-member size (N4280, also provided by libc++ 3.6+ in c++1z mode)
#if ((__cplusplus <= 201402L) || (__clang_major__ == 3 && __clang_minor__ <= 5)) && \
	!defined(FLOOR_COMPUTE_NO_NON_MEMBER_SIZE)
template <class C> constexpr auto size(const C& c) noexcept -> decltype(c.size()) {
	return c.size();
}
template <class T, size_t N> constexpr size_t size(const T (&)[N]) noexcept {
	return N;
}
#endif

#endif
