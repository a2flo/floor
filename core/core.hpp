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

#ifndef __FLOOR_CORE_HPP__
#define __FLOOR_CORE_HPP__

#include <floor/core/cpp_headers.hpp>
#include <floor/math/vector_lib.hpp>
#include <floor/math/matrix4.hpp>
#include <floor/core/file_io.hpp>
#include <random>

class core {
public:
	//
	static void init();
	
	// 3d math functions
	static ipnt get_2d_from_3d(const float3& vec, const matrix4f& mview, const matrix4f& mproj, const int4& viewport);
	static float3 get_3d_from_2d(const pnt& p, const matrix4f& mview, const matrix4f& mproj, const int4& viewport);
	
	static void compute_normal(const float3& v1, const float3& v2, const float3& v3, float3& normal);
	static void compute_normal_tangent_binormal(const float3& v1, const float3& v2, const float3& v3,
												float3& normal, float3& binormal, float3& tangent,
												const coord& t1, const coord& t2, const coord& t3);
	
	// random functions
	static void set_random_seed(const unsigned int& seed);
	
	//! random number in [0, max]
	template <typename int_type, typename enable_if<is_integral<int_type>::value, int>::type = 0>
	static int_type rand(int_type max = numeric_limits<int_type>::max()) {
		uniform_int_distribution<int_type> dist((int_type)0, max);
		return dist(gen);
	}
	
	//! random number in [min, max]
	template <typename int_type, typename enable_if<is_integral<int_type>::value, int>::type = 0>
	static int_type rand(const int_type& min, const int_type& max) {
		uniform_int_distribution<int_type> dist(min, max);
		return dist(gen);
	}
	
	//! random number in [0, max)
	template <typename fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	static fp_type rand(fp_type max = numeric_limits<fp_type>::max()) {
		uniform_real_distribution<fp_type> dist((fp_type)0, max);
		return dist(gen);
	}
	
	//! random number in [min, max)
	template <typename fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	static fp_type rand(const fp_type& min, const fp_type& max) {
		uniform_real_distribution<fp_type> dist(min, max);
		return dist(gen);
	}
	
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
	
	//! converts a string to a hex escaped string: "Hello" -> "\x48\x65\x6C\x6C\x6F"
	static string str_hex_escape(const string& str);
	//! converts generic 8-bit data (const char*) to a hex escaped string
	//! NOTE: if size is 0, this assumes data is \0 escaped and will stop at that point
	static string cptr_hex_escape(const char* data, const size_t size = 0);
	
	// folder/path functions
	static map<string, file_io::FILE_TYPE> get_file_list(const string& directory,
														 const string file_extension = "",
														 const bool always_get_folders = false);
	//! extracts the path from in_path and tries to condense it if it contains "../"
	static string strip_path(const string& in_path);
	//! extracts the filename from in_path
	static string strip_filename(const string& in_path);
	
	// system functions
	static void system(const string& cmd);
	static void system(const string& cmd, string& output);
	
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
	static uint32_t unix_timestamp();
	template <typename clock_type>
	static inline uint32_t unix_timestamp(const chrono::time_point<clock_type>& time_point) {
		return (uint32_t)chrono::duration_cast<chrono::seconds>(time_point.time_since_epoch()).count();
	}
	
	static uint32_t get_hw_thread_count();
	
	//! sets the name/label of the current thread to "thread_name" (only works with pthreads)
	static void set_current_thread_name(const string& thread_name);
	
	//! returns the name/label of the current thread (only works with pthreads)
	static string get_current_thread_name();
	
protected:
	// static class
	core(const core&) = delete;
	~core() = delete;
	core& operator=(const core&) = delete;
	
	static random_device rd;
	static mt19937 gen;

};

// TODO: move this somewhere appropriate
struct rect {
	union {
		struct {
			unsigned int x1;
			unsigned int y1;
			unsigned int x2;
			unsigned int y2;
		};
	};
	
	void set(const unsigned int& x1_, const unsigned int& y1_, const unsigned int& x2_, const unsigned int& y2_) {
		x1 = x1_; y1 = y1_; x2 = x2_; y2 = y2_;
	}
	
	friend ostream& operator<<(ostream& output, const rect& r) {
		output << "(" << r.x1 << ", " << r.y1 << ") x (" << r.x2 << ", " << r.y2 << ")";
		return output;
	}
	
	rect() : x1(0), y1(0), x2(0), y2(0) {}
	rect(const rect& r) : x1(r.x1), y1(r.y1), x2(r.x2), y2(r.y2) {}
	rect(const unsigned int& x1_, const unsigned int& y1_, const unsigned int& x2_, const unsigned int& y2_) : x1(x1_), y1(y1_), x2(x2_), y2(y2_) {}
	~rect() {}
};

#endif
