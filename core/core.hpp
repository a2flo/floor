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

#ifndef __FLOOR_CORE_HPP__
#define __FLOOR_CORE_HPP__

#include "core/cpp_headers.hpp"
#include "core/basic_math.hpp"
#include "core/vector2.hpp"
#include "core/vector3.hpp"
#include "core/vector4.hpp"
#include "core/matrix4.hpp"
#include "core/file_io.hpp"
#include <random>

/*! @class core
 *  @brief core stuff
 */

class FLOOR_API core {
public:
	core() = delete;
	~core() = delete;
	
	//
	static void init();
	
	// 3d math functions
	static ipnt get_2d_from_3d(const float3& vec, const matrix4f& mview, const matrix4f& mproj, const int4& viewport);
	static float3 get_3d_from_2d(const pnt& p, const matrix4f& mview, const matrix4f& mproj, const int4& viewport);
	
	static void compute_normal(const float3& v1, const float3& v2, const float3& v3, float3& normal);
	static void compute_normal_tangent_binormal(const float3& v1, const float3& v2, const float3& v3,
												float3& normal, float3& binormal, float3& tangent,
												const coord& t1, const coord& t2, const coord& t3);

	// misc math functions
	static size_t lcm(size_t v1, size_t v2);
	static size_t gcd(size_t v1, size_t v2);
	
	static unsigned int next_pot(const unsigned int& num);
	static unsigned long long int next_pot(const unsigned long long int& num);
	
	template <typename T> static T clamp(const T& var, const T& min, const T& max) {
		return (var < min ? min : (var > max ? max : var));
	}
	
	static float wrap(const float& var, const float& max) {
		return (var < 0.0f ? (max - fmodf(fabs(var), max)) : fmodf(var, max));
	}
	
	static int rand(const int& max);
	static int rand(const int& min, const int& max);
	static unsigned int rand(const unsigned int& max);
	static unsigned int rand(const unsigned int& min, const unsigned int& max);
	static float rand(const float& max);
	static float rand(const float& min, const float& max);
	static void set_random_seed(const unsigned int& seed);
	
	template <typename T> static set<T> power_set(set<T> input_set) {
		if(input_set.empty()) return set<T> {};
		
		const T elem(*input_set.begin());
		input_set.erase(elem);
		
		set<T> subset(power_set(input_set));
		set<T> ret(subset);
		ret.insert(elem);
		for(const auto& sub_elem : subset) {
			ret.insert(elem + sub_elem);
		}
		
		return ret;
	}
	
	template <typename T> static T interpolate(const T& v0, const T& v1, const T& interp) {
		return ((v1 - v0) * interp + v0);
	}
	
	// string functions
	static string find_and_replace(const string& str, const string& find, const string& repl);
	static void find_and_replace(string& str, const string& find, const string& repl); // inline find and replace
	static string trim(const string& str);
	static string escape_string(const string& str);
	static vector<string> tokenize(const string& src, const char& delim);
	static vector<string> tokenize(const string& src, const string& delim);
	static void str_to_lower_inplace(string& str);
	static void str_to_upper_inplace(string& str);
	static string str_to_lower(const string& str);
	static string str_to_upper(const string& str);
	static string encode_url(const string& url);
	
	// folder/path functions
	static map<string, file_io::FILE_TYPE> get_file_list(const string& directory,
														 const string file_extension = "",
														 const bool always_get_folders = false);
	static string strip_path(const string& in_path);
	static string strip_filename(const string& in_path);
	
	// system functions
	static void system(const string& cmd);
	static void system(const string& cmd, string& output);
	
	// misc functions
	template <class container_type>
	static inline void erase_if(container_type& container,
								function<bool(const typename container_type::iterator&)> erase_if_function) {
		for(auto iter = container.begin(); iter != container.end();) {
			if(erase_if_function(iter)) {
				iter = container.erase(iter);
			}
			else ++iter;
		}
	}
	
	static uint32_t unix_timestamp();
	template <typename clock_type>
	static inline uint32_t unix_timestamp(const chrono::time_point<clock_type>& time_point) {
		return (uint32_t)chrono::duration_cast<chrono::seconds>(time_point.time_since_epoch()).count();
	}
	
protected:
	// random_device with libc++ on windows/mingw is not supported right now (no /dev/urandom)
	// -> use default mt19937 and seed+warm-up manually
#if !(defined(__clang__) && defined(WIN_UNIXENV))
	static random_device rd;
#endif
	static mt19937 gen;

};

#endif
