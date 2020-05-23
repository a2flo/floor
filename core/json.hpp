/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

namespace json {
	struct json_value;
	typedef unordered_map<string, json_value> json_object;
	typedef vector<json_value> json_array;
	
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
				json_object members;
			} object;
			struct {
				json_array values;
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
			return { true, *(const uint64_t*)&int_number };
		}
		template <typename T, enable_if_t<is_same<T, int32_t>::value>* = nullptr>
		pair<bool, int32_t> get() const {
			if(type != VALUE_TYPE::INT_NUMBER) {
				return { false, 0 };
			}
			return { true, int32_t(int_number < 0 ?
								   std::max(int_number, int64_t(INT32_MIN)) :
								   std::min(int_number, int64_t(INT32_MAX))) };
		}
		template <typename T, enable_if_t<is_same<T, uint32_t>::value>* = nullptr>
		pair<bool, uint32_t> get() const {
			if(type != VALUE_TYPE::INT_NUMBER) {
				return { false, 0 };
			}
			return { true, uint32_t(std::min(*(const uint64_t*)&int_number, uint64_t(UINT32_MAX))) };
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
		template <typename T, enable_if_t<is_same<T, json_object>::value>* = nullptr>
		pair<bool, json_object> get() const {
			if(type != VALUE_TYPE::OBJECT) {
				return { false, {} };
			}
			return { true, object.members };
		}
		template <typename T, enable_if_t<is_same<T, json_array>::value>* = nullptr>
		pair<bool, json_array> get() const {
			if(type != VALUE_TYPE::ARRAY) {
				return { false, {} };
			}
			return { true, array.values };
		}
		
		//! gets this value as type "T" or throws if this value is not of the specified type
		template <typename T>
		T get_or_throw() const {
			const auto ret = get<T>();
			if (!ret.first) {
				throw runtime_error("json_value is not of type "s + typeid(T).name());
			}
			return ret.second;
		}
		
		void print(const uint32_t depth = 0) const;
		
		constexpr json_value() noexcept : type(VALUE_TYPE::NULL_VALUE), int_number(0) {}
		json_value(json_value&& val);
		json_value& operator=(json_value&& val);
		json_value(const json_value& val);
		json_value& operator=(const json_value& val);
		// init for null/true/false and default init + allocation for all else
		json_value(const VALUE_TYPE& value_type);
		// init as string
		json_value(const string& str_) : json_value(VALUE_TYPE::STRING) { str = str_; }
		// init as signed integer
		json_value(const int64_t& val) : json_value(VALUE_TYPE::INT_NUMBER) { int_number = val; }
		// init as unsigned integer
		json_value(const uint64_t& val) : json_value(VALUE_TYPE::INT_NUMBER) { int_number = *(const int64_t*)&val; }
		// init as single precision floating point
		json_value(const float& val) : json_value(VALUE_TYPE::FP_NUMBER) { fp_number = (double)val; }
		// init as double precision floating point
		json_value(const double& val) : json_value(VALUE_TYPE::FP_NUMBER) { fp_number = val; }
		~json_value();
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
		
		//! dumps the document to cout
		void print() const {
			root.print();
		}
	};
	
	//! reads the json file specified by 'filename' and creates a json document from it
	document create_document(const string& filename);
	
	//! creates a json document from the in-memory json data
	//! 'identifier' is used for error reporting/identification
	document create_document_from_string(const string& json_data, const string identifier = "");

} // json


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
template<> struct json::document::default_value<uint32_t> {
	static constexpr uint32_t def() { return 0u; }
};
template<> struct json::document::default_value<int32_t> {
	static constexpr int32_t def() { return 0; }
};
template<> struct json::document::default_value<bool> {
	static constexpr bool def() { return false; }
};
template<> struct json::document::default_value<json::json_object> {
	static json::json_object def() { return {}; }
};
template<> struct json::document::default_value<json::json_array> {
	static json::json_array def() { return {}; }
};

template<> string json::document::get<string>(const string& path, const string default_value) const;
template<> float json::document::get<float>(const string& path, const float default_value) const;
template<> double json::document::get<double>(const string& path, const double default_value) const;
template<> uint64_t json::document::get<uint64_t>(const string& path, const uint64_t default_value) const;
template<> int64_t json::document::get<int64_t>(const string& path, const int64_t default_value) const;
template<> uint32_t json::document::get<uint32_t>(const string& path, const uint32_t default_value) const;
template<> int32_t json::document::get<int32_t>(const string& path, const int32_t default_value) const;
template<> bool json::document::get<bool>(const string& path, const bool default_value) const;
template<> json::json_object json::document::get<json::json_object>(const string& path, const json::json_object default_value) const;
template<> json::json_array json::document::get<json::json_array>(const string& path, const json::json_array default_value) const;

#endif
