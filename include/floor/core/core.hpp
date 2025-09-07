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

#include <floor/math/vector_lib.hpp>
#include <floor/math/matrix4.hpp>
#include <floor/core/file_io.hpp>
#include <floor/constexpr/ext_traits.hpp>
#include <random>
#include <set>
#include <map>

namespace fl::core {
using namespace std::literals;

	//! converts (projects) a 3D position to a 2D screen position
	float3 get_3d_from_2d(const uint2& p, const matrix4f& mview, const matrix4f& mproj, const int4& viewport);
	//! inverse: converts a 2D screen position to a 3D position
	int2 get_2d_from_3d(const float3& vec, const matrix4f& mview, const matrix4f& mproj, const int4& viewport);
	
	//! computes the normal of a triangle specified by v1, v2 and v3
	void compute_normal(const float3& v1, const float3& v2, const float3& v3, float3& normal);

	//! computes the normal, binormal and tangent of a triangle specified by v1, v2 and v3 and their texture coordinates
	void compute_normal_tangent_binormal(const float3& v1, const float3& v2, const float3& v3,
										 float3& normal, float3& binormal, float3& tangent,
										 const float2& t1, const float2& t2, const float2& t3);
	
	// random functions
	void set_random_seed(const uint32_t& seed);

	//! for internal uses only
	std::mt19937& _get_gen();

	//! random number in [0, max]
	template <typename int_type> requires ext::is_integral_v<int_type>
	static inline int_type rand(int_type max = std::numeric_limits<int_type>::max()) {
		std::uniform_int_distribution<int_type> dist((int_type)0, max);
		return dist(_get_gen());
	}
	
	//! random number in [min, max]
	template <typename int_type> requires ext::is_integral_v<int_type>
	static inline int_type rand(const int_type& min, const int_type& max) {
		std::uniform_int_distribution<int_type> dist(min, max);
		return dist(_get_gen());
	}
	
	//! random number in [0, max)
	template <typename fp_type> requires ext::is_floating_point_v<fp_type>
	static inline fp_type rand(fp_type max = std::numeric_limits<fp_type>::max()) {
		std::uniform_real_distribution<fp_type> dist((fp_type)0, max);
		return dist(_get_gen());
	}
	
	//! random number in [min, max)
	template <typename fp_type> requires ext::is_floating_point_v<fp_type>
	static inline fp_type rand(const fp_type& min, const fp_type& max) {
		std::uniform_real_distribution<fp_type> dist(min, max);
		return dist(_get_gen());
	}
	
	template <typename T> static inline std::set<T> power_set(std::set<T> input_set) {
		if(input_set.empty()) return std::set<T> {};
		
		const T elem(*input_set.begin());
		input_set.erase(elem);
		
		std::set<T> subset(power_set(input_set));
		std::set<T> ret(subset);
		ret.insert(elem);
		for(const auto& sub_elem : subset) {
			ret.insert(elem + sub_elem);
		}
		
		return ret;
	}
	
	// string functions
	std::string find_and_replace(const std::string& str, const std::string& find, const std::string& repl);
	void find_and_replace(std::string& str, const std::string& find, const std::string& repl); // inline find and replace
	std::string find_and_replace_once(const std::string& str, const std::string& find_str, const std::string& repl_str, const size_t start_pos = 0);
	void find_and_replace_once(std::string& str, const std::string& find_str, const std::string& repl_str, const size_t start_pos = 0);
	std::string trim(const std::string& str);
	std::string escape_string(const std::string& str);
	std::vector<std::string> tokenize(const std::string& src, const char& delim);
	std::vector<std::string> tokenize(const std::string& src, const std::string& delim);
	std::vector<std::string_view> tokenize_sv(const std::string_view& src, const char& delim);
	std::vector<std::string_view> tokenize_sv(const std::string_view& src, const std::string_view& delim);
	void str_to_lower_inplace(std::string& str);
	void str_to_upper_inplace(std::string& str);
	std::string str_to_lower(const std::string& str);
	std::string str_to_upper(const std::string& str);
	std::string encode_url(const std::string& url);
	
	//! converts a string to a hex escaped string: "Hello" -> "\x48\x65\x6C\x6C\x6F"
	std::string str_hex_escape(const std::string& str);
	//! converts generic 8-bit data (const char*) to a hex escaped string
	//! NOTE: if size is 0, this assumes data is \0 escaped and will stop at that point
	std::string cptr_hex_escape(const char* data, const size_t size = 0);
	
	// folder/path functions
	std::map<std::string, file_io::FILE_TYPE> get_file_list(const std::string& directory,
												  const std::string file_extension = "",
												  const bool always_get_folders = false);
	//! extracts the path from in_path and tries to condense it if it contains "../"
	std::string strip_path(const std::string& in_path);
	//! extracts the filename from in_path
	std::string strip_filename(const std::string& in_path);
	//! creates a temporary file name, optionally prefixed by 'prefix' and suffixed by 'suffix', and returns it
	std::string create_tmp_file_name(const std::string prefix = "", const std::string suffix = "");
	//! creates an array of temporary file names using, each prefixed by a corresponding element in 'prefixes'
	//! and suffixed by a corresponding element in 'suffixes', and returns the array
	template <size_t count>
	static inline std::array<std::string, count> create_tmp_file_names(const std::array<const char*, count>& prefixes,
															 const std::array<const char*, count>& suffixes) {
		std::array<std::string, count> ret;
		for(size_t i = 0; i < count; ++i) {
			ret[i] = create_tmp_file_name(prefixes[i], suffixes[i]);
		}
		return ret;
	}
	//! converts an input string to a string that can be used as a file name (mostly ASCII)
	std::string to_file_name(const std::string& str);
	//! for use on windows: expands %ENV% variables in the given path/string
	std::string expand_path_with_env(const std::string& in);
	
	// system functions
	void system(const std::string& cmd);
	void system(const std::string& cmd, std::string& output);
	
	// container functions
	template <class container_type>
	static inline void erase_if(container_type& container,
								std::function<bool(const typename container_type::iterator&)> erase_if_function) {
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
		std::vector<typename container_type::key_type> ret;
		ret.reserve(container.size());
		for(const auto& entry : container) {
			ret.emplace_back(entry.first);
		}
		floor_return_no_nrvo(ret);
	}
	
	//! returns a vector of the values of the specified (associative) container
	template <class container_type>
	static inline auto values(const container_type& container) {
		std::vector<typename container_type::value_type> ret;
		ret.reserve(container.size());
		for(const auto& entry : container) {
			ret.emplace_back(entry.second);
		}
		floor_return_no_nrvo(ret);
	}
	
	// misc functions
	uint32_t unix_timestamp();
	uint64_t unix_timestamp_ms();
	uint64_t unix_timestamp_us();
	template <typename clock_type>
	static inline uint32_t unix_timestamp(const std::chrono::time_point<clock_type>& time_point) {
		return (uint32_t)std::chrono::duration_cast<std::chrono::seconds>(time_point.time_since_epoch()).count();
	}
	
	//! returns the total amount of CPU/system memory in bytes,
	//! returns 0 if the query failed or is unimplemented
	uint64_t get_total_system_memory();
	
	//! returns the currently free/available amount of CPU/system memory in bytes,
	//! returns 0 if the query failed or is unimplemented
	uint64_t get_free_system_memory();

#if defined(__WINDOWS__)
	//! returns the Windows version as { major, minor, build, revision }
	//! invalid:     { 0, 0, 0, 0 }
	//! Windows 10:  { 10, 0, ... }
	//! Windows 11:  { 11, 0, ... }
	uint4 get_windows_version();
#endif

	//! returns a description of the last system error (errno/GetLastError())
#if !defined(__WINDOWS__)
	static inline std::string get_system_error() {
		const auto err = errno;
		return strerror(err) + " ("s + std::to_string(err) + ")";
	}
#else
	std::string get_system_error();
#endif
	
	//! returns true if the CPU has FMA instruction support
	bool cpu_has_fma();
	//! returns true if the CPU has AVX instruction support
	bool cpu_has_avx();
	//! returns true if the CPU has AVX2 instruction support
	bool cpu_has_avx2();
	//! returns true if the CPU has general AVX-512 instruction support
	bool cpu_has_avx512();
	//! returns true if the CPU has AVX-512 with IFMA, VBMI, VBMI2, VAES, BITALG, VPCLMULQDQ, GFNI, VNNI, VPOPCNTDQ, BF16 instruction support
	//! NOTE: this is used to determine Host-Compute X86_TIER_5 support
	bool cpu_has_avx512_tier_5();
	//! returns the CPU name (if available)
	std::string get_cpu_name();

} // namespace fl::core
