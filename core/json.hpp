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

#ifndef __FLOOR_JSON_HPP__
#define __FLOOR_JSON_HPP__

#include <floor/core/cpp_headers.hpp>

class json {
public:
	struct json_member;
	
	//! json value (keyword, object, array, number or string)
	struct json_value {
		enum class VALUE_TYPE : uint32_t {
			NULL_VALUE,
			TRUE_VALUE,
			FALSE_VALUE,
			OBJECT,
			ARRAY,
			INT_NUMBER,
			FP_NUMBER,
			STRING,
		};
		VALUE_TYPE type;
		
		union {
			struct {
				vector<json_member> members;
			} object;
			struct {
				vector<json_value> values;
			} array;
			struct {
				int64_t int_number;
			};
			struct {
				double fp_number;
			};
			struct {
				string str;
			};
		};
		
		//! returns the value of this value if its type matches the specified type T,
		//! returning it as <true, value>, or returning <false, 0> if the type doesn't match
		template <typename T, enable_if_t<is_same<T, nullptr_t>::value>* = nullptr>
		pair<bool, nullptr_t> get() const {
			if(type != VALUE_TYPE::NULL_VALUE) {
				return { false, nullptr };
			}
			return { true, nullptr };
		}
		template <typename T, enable_if_t<is_same<T, bool>::value>* = nullptr>
		pair<bool, bool> get() const {
			if(type != VALUE_TYPE::TRUE_VALUE &&
			   type != VALUE_TYPE::FALSE_VALUE) {
				return { false, 0 };
			}
			return { true, (type == VALUE_TYPE::TRUE_VALUE) };
		}
		template <typename T, enable_if_t<is_same<T, int64_t>::value>* = nullptr>
		pair<bool, int64_t> get() const {
			if(type != VALUE_TYPE::INT_NUMBER) {
				return { false, 0 };
			}
			return { true, int_number };
		}
		template <typename T, enable_if_t<is_same<T, uint64_t>::value>* = nullptr>
		pair<bool, uint64_t> get() const {
			if(type != VALUE_TYPE::INT_NUMBER) {
				return { false, 0 };
			}
			return { true, *(uint64_t*)&int_number };
		}
		template <typename T, enable_if_t<is_same<T, float>::value>* = nullptr>
		pair<bool, float> get() const {
			if(type != VALUE_TYPE::FP_NUMBER) {
				return { false, 0.0f };
			}
			return { true, (float)fp_number };
		}
		template <typename T, enable_if_t<is_same<T, double>::value>* = nullptr>
		pair<bool, double> get() const {
			if(type != VALUE_TYPE::FP_NUMBER) {
				return { false, 0.0 };
			}
			return { true, fp_number };
		}
		template <typename T, enable_if_t<is_same<T, string>::value>* = nullptr>
		pair<bool, string> get() const {
			if(type != VALUE_TYPE::STRING) {
				return { false, "" };
			}
			return { true, str };
		}
		
		constexpr json_value() noexcept : type(VALUE_TYPE::NULL_VALUE), int_number(0) {}
		json_value(const VALUE_TYPE& value_type);
		json_value(json_value&& val);
		json_value& operator=(json_value&& val);
		~json_value();
	};
	
	//! json member -> "key" : <value>
	struct json_member {
		string key;
		json_value value;
		
		json_member(json_member&& member_) : key(move(member_.key)), value(move(member_.value)) {}
		json_member(string&& key_, json_value&& value_) : key(move(key_)), value(move(value_)) {}
	};
	
	//! json document, root is always a json value
	struct document {
	private:
		template<typename T> struct default_value {
			static T def() { return T(); }
		};
		
	public:
		json_value root;
		bool valid { false };
		
		//! returns the value of the key specified by path "node.subnode.key",
		//! or the root node if path is an empty string
		template<typename T> T get(const string& path,
								   const T default_val = default_value<T>::def()) const;
	};
	
	//! reads the json file specified by 'filename' and creates a json document from it
	static document create_document(const string& filename);
	
	//! creates a json document from the in-memory json data
	//! 'identifier' is used for error reporting/identification
	static document create_document_from_string(const string& json_data,
												const string identifier = "");
	
protected:
	// static class
	json(const json&) = delete;
	~json() = delete;
	json& operator=(const json&) = delete;

};


// explicit specializations of the json document getters
template<> struct json::document::default_value<string> {
	static string def() { return ""; }
};
template<> struct json::document::default_value<float> {
	static constexpr float def() { return 0.0f; }
};
template<> struct json::document::default_value<double> {
	static constexpr double def() { return 0.0; }
};
template<> struct json::document::default_value<uint64_t> {
	static constexpr uint64_t def() { return 0ull; }
};
template<> struct json::document::default_value<int64_t> {
	static constexpr int64_t def() { return 0ll; }
};
template<> struct json::document::default_value<bool> {
	static constexpr bool def() { return false; }
};

template<> string json::document::get<string>(const string& path, const string default_value) const;
template<> float json::document::get<float>(const string& path, const float default_value) const;
template<> double json::document::get<double>(const string& path, const double default_value) const;
template<> uint64_t json::document::get<uint64_t>(const string& path, const uint64_t default_value) const;
template<> int64_t json::document::get<int64_t>(const string& path, const int64_t default_value) const;
template<> bool json::document::get<bool>(const string& path, const bool default_value) const;

#endif
