/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#include <floor/core/cpp_headers.hpp>
#include <floor/math/vector_lib.hpp>
#include <floor/math/matrix4.hpp>
#include <floor/core/file_io.hpp>
#include <floor/constexpr/ext_traits.hpp>
#include <random>

namespace core {
	// 3d math functions
	int2 get_2d_from_3d(const float3& vec, const matrix4f& mview, const matrix4f& mproj, const int4& viewport);
	float3 get_3d_from_2d(const uint2& p, const matrix4f& mview, const matrix4f& mproj, const int4& viewport);
	
	void compute_normal(const float3& v1, const float3& v2, const float3& v3, float3& normal);
	void compute_normal_tangent_binormal(const float3& v1, const float3& v2, const float3& v3,
										 float3& normal, float3& binormal, float3& tangent,
										 const float2& t1, const float2& t2, const float2& t3);
	
	// random functions
	void set_random_seed(const uint32_t& seed);

	//! for internal uses only
	mt19937& _get_gen();

	//! random number in [0, max]
	template <typename int_type> requires ext::is_integral_v<int_type>
	static inline int_type rand(int_type max = numeric_limits<int_type>::max()) {
		uniform_int_distribution<int_type> dist((int_type)0, max);
		return dist(_get_gen());
	}
	
	//! random number in [min, max]
	template <typename int_type> requires ext::is_integral_v<int_type>
	static inline int_type rand(const int_type& min, const int_type& max) {
		uniform_int_distribution<int_type> dist(min, max);
		return dist(_get_gen());
	}
	
	//! random number in [0, max)
	template <typename fp_type> requires ext::is_floating_point_v<fp_type>
	static inline fp_type rand(fp_type max = numeric_limits<fp_type>::max()) {
		uniform_real_distribution<fp_type> dist((fp_type)0, max);
		return dist(_get_gen());
	}
	
	//! random number in [min, max)
	template <typename fp_type> requires ext::is_floating_point_v<fp_type>
	static inline fp_type rand(const fp_type& min, const fp_type& max) {
		uniform_real_distribution<fp_type> dist(min, max);
		return dist(_get_gen());
	}
	
	template <typename T> static inline set<T> power_set(set<T> input_set) {
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
	
	// string functions
	string find_and_replace(const string& str, const string& find, const string& repl);
	void find_and_replace(string& str, const string& find, const string& repl); // inline find and replace
	string find_and_replace_once(const string& str, const string& find_str, const string& repl_str, const size_t start_pos = 0);
	void find_and_replace_once(string& str, const string& find_str, const string& repl_str, const size_t start_pos = 0);
	string trim(const string& str);
	string escape_string(const string& str);
	vector<string> tokenize(const string& src, const char& delim);
	vector<string> tokenize(const string& src, const string& delim);
	void str_to_lower_inplace(string& str);
	void str_to_upper_inplace(string& str);
	string str_to_lower(const string& str);
	string str_to_upper(const string& str);
	string encode_url(const string& url);
	
	//! converts a string to a hex escaped string: "Hello" -> "\x48\x65\x6C\x6C\x6F"
	string str_hex_escape(const string& str);
	//! converts generic 8-bit data (const char*) to a hex escaped string
	//! NOTE: if size is 0, this assumes data is \0 escaped and will stop at that point
	string cptr_hex_escape(const char* data, const size_t size = 0);
	
	// folder/path functions
	map<string, file_io::FILE_TYPE> get_file_list(const string& directory,
												  const string file_extension = "",
												  const bool always_get_folders = false);
	//! extracts the path from in_path and tries to condense it if it contains "../"
	string strip_path(const string& in_path);
	//! extracts the filename from in_path
	string strip_filename(const string& in_path);
	//! creates a temporary file name, optionally prefixed by 'prefix' and suffixed by 'suffix', and returns it
	string create_tmp_file_name(const string prefix = "", const string suffix = "");
	//! creates an array of temporary file names using, each prefixed by a corresponding element in 'prefixes'
	//! and suffixed by a corresponding element in 'suffixes', and returns the array
	template <size_t count>
	static inline array<string, count> create_tmp_file_names(const array<const char*, count>& prefixes,
															 const array<const char*, count>& suffixes) {
		array<string, count> ret;
		for(size_t i = 0; i < count; ++i) {
			ret[i] = create_tmp_file_name(prefixes[i], suffixes[i]);
		}
		return ret;
	}
	//! converts an input string to a string that can be used as a file name (mostly ASCII)
	string to_file_name(const string& str);
	//! for use on windows: expands %ENV% variables in the given path/string
	string expand_path_with_env(const string& in);
	
	// system functions
	void system(const string& cmd);
	void system(const string& cmd, string& output);
	
	// container functions
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
	
	//! returns the iterator returned by find when searching for "needle" (either an object or a *predicate function)
	template <class container_type, typename needle_type>
	static inline auto find(const container_type& container, needle_type&& needle) {
		return std::find(cbegin(container), cend(container), needle);
	}
	
	//! returns true if container contains "needle" (either an object or a *predicate function)
	template <class container_type, typename needle_type>
	static inline bool contains(const container_type& container, needle_type&& needle) {
		return (std::find(cbegin(container), cend(container), needle) != cend(container));
	}
	
	//! returns a vector of the keys of the specified (associative) container
	template <class container_type>
	static inline auto keys(const container_type& container) {
		vector<typename container_type::key_type> ret;
		ret.reserve(container.size());
		for(const auto& entry : container) {
			ret.emplace_back(entry.first);
		}
		return ret;
	}
	
	//! returns a vector of the values of the specified (associative) container
	template <class container_type>
	static inline auto values(const container_type& container) {
		vector<typename container_type::value_type> ret;
		ret.reserve(container.size());
		for(const auto& entry : container) {
			ret.emplace_back(entry.second);
		}
		return ret;
	}
	
	// misc functions
	uint32_t unix_timestamp();
	uint64_t unix_timestamp_ms();
	uint64_t unix_timestamp_us();
	template <typename clock_type>
	static inline uint32_t unix_timestamp(const chrono::time_point<clock_type>& time_point) {
		return (uint32_t)chrono::duration_cast<chrono::seconds>(time_point.time_since_epoch()).count();
	}
	
	//! returns the number of hardware threads ("logical CPU cores")
	uint32_t get_hw_thread_count();

	//! returns the number of physical CPU cores
	uint32_t get_physical_core_count();

	//! sets the current threads affinity to the specified "affinity"
	//! NOTE: 0 represents no affinity, 1 is CPU core #0, ...
	void set_thread_affinity(const uint32_t affinity);

	//! sets the name/label of the current thread to "thread_name" (only works with pthreads)
	void set_current_thread_name(const string& thread_name);
	
	//! returns the name/label of the current thread (only works with pthreads)
	string get_current_thread_name();

#if defined(__WINDOWS__)
	//! returns the Windows version as { major, minor, build, revision }
	//! invalid:     { 0, 0, 0, 0 }
	//! Windows 7:   { 6, 1, ... }
	//! Windows 8:   { 6, 2, ... }
	//! Windows 8.1: { 6, 3, ... }
	//! Windows 10:  { 10, 0, ... }
	uint4 get_windows_version();

	//! returns true if running Windows 8 or higher
	bool is_windows_8_or_higher();
#endif
	
	//! returns true if the cpu has fma instruction support
	bool cpu_has_fma();
	//! returns true if the cpu has avx instruction support
	bool cpu_has_avx();
	//! returns true if the cpu has avx2 instruction support
	bool cpu_has_avx2();
	//! returns true if the cpu has avx-512 instruction support
	bool cpu_has_avx512();

}

#endif
