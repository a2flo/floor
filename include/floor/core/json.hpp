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

#include <floor/core/flat_map.hpp>
#include <variant>
#include <iostream>
#include <fstream>

namespace fl::json {
using namespace std::literals;

	struct json_value;
	using json_object = fl::flat_map<std::string, json_value>;
	using json_array = std::vector<json_value>;
	
	//! json value (keyword, object, array, number or string)
	struct json_value {
		//! variant of all possible value types
		using value_container_t = std::variant<std::nullptr_t, json_object, json_array, int64_t, double, std::string, bool>;
		
		//! each value type as an enum
		//! NOTE: order in here matches the order in value_container_t
		enum class VALUE_TYPE : uint32_t {
			NULL_VALUE,
			OBJECT,
			ARRAY,
			INT_NUMBER,
			FP_NUMBER,
			STRING,
			BOOL_VALUE,
		};
		
		//! the actual value storage
		value_container_t value;
		
		//! returns the active/underlying type of "value"
		constexpr VALUE_TYPE get_type() const {
			if (auto idx = value.index(); idx != std::variant_npos) {
				return (VALUE_TYPE)idx;
			}
			return VALUE_TYPE::NULL_VALUE;
		}
		
		//! returns the value of this value if its type matches the specified type T,
		//! returning it as <true, value>, or returning <false, 0> if the type doesn't match
		template <typename T> requires std::is_same_v<T, std::nullptr_t>
		std::pair<bool, std::nullptr_t> get() const {
			if (auto val_ptr = std::get_if<std::nullptr_t>(&value); val_ptr) {
				return { true, *val_ptr };
			}
			return { false, nullptr };
		}
		template <typename T> requires std::is_same_v<T, bool>
		std::pair<bool, bool> get() const {
			if (auto val_ptr = std::get_if<bool>(&value); val_ptr) {
				return { true, *val_ptr };
			}
			return { false, false };
		}
		template <typename T> requires std::is_same_v<T, int64_t>
		std::pair<bool, int64_t> get() const {
			if (auto val_ptr = std::get_if<int64_t>(&value); val_ptr) {
				return { true, *val_ptr };
			}
			return { false, 0 };
		}
		template <typename T> requires std::is_same_v<T, uint64_t>
		std::pair<bool, uint64_t> get() const {
			if (auto val_ptr = std::get_if<int64_t>(&value); val_ptr) {
				return { true, std::bit_cast<uint64_t>(*val_ptr) };
			}
			return { false, 0u };
		}
		template <typename T> requires std::is_same_v<T, int32_t>
		std::pair<bool, int32_t> get() const {
			if (auto val_ptr = std::get_if<int64_t>(&value); val_ptr) {
				const auto int_number = *val_ptr;
				return { true, int32_t(int_number < 0 ?
									   std::max(int_number, int64_t(INT32_MIN)) :
									   std::min(int_number, int64_t(INT32_MAX))) };
			}
			return { false, 0 };
		}
		template <typename T> requires std::is_same_v<T, uint32_t>
		std::pair<bool, uint32_t> get() const {
			if (auto val_ptr = std::get_if<int64_t>(&value); val_ptr) {
				return { true, uint32_t(std::min(std::bit_cast<uint64_t>(*val_ptr), uint64_t(UINT32_MAX))) };
			}
			return { false, 0u };
		}
		template <typename T> requires std::is_same_v<T, float>
		std::pair<bool, float> get() const {
			if (auto val_ptr = std::get_if<double>(&value); val_ptr) {
				return { true, float(*val_ptr) };
			}
			return { false, 0.0f };
		}
		template <typename T> requires std::is_same_v<T, double>
		std::pair<bool, double> get() const {
			if (auto val_ptr = std::get_if<double>(&value); val_ptr) {
				return { true, *val_ptr };
			}
			return { false, 0.0 };
		}
		template <typename T> requires std::is_same_v<T, std::string>
		std::pair<bool, std::string> get() const {
			if (auto val_ptr = std::get_if<std::string>(&value); val_ptr) {
				return { true, *val_ptr };
			}
			return { false, "" };
		}
		template <typename T> requires std::is_same_v<T, json_object>
		std::pair<bool, json_object> get() const {
			if (auto val_ptr = std::get_if<json_object>(&value); val_ptr) {
				return { true, *val_ptr };
			}
			return { false, {} };
		}
		template <typename T> requires std::is_same_v<T, json_array>
		std::pair<bool, json_array> get() const {
			if (auto val_ptr = std::get_if<json_array>(&value); val_ptr) {
				return { true, *val_ptr };
			}
			return { false, {} };
		}
		
		//! gets this value as type "T" or throws if this value is not of the specified type
		template <typename T>
		T get_or_throw() const {
			auto ret = get<T>();
			if (!ret.first) {
				throw std::runtime_error("json_value is not of type "s + typeid(T).name());
			}
			return ret.second;
		}
		
		void print(std::ostream& stream = std::cout, const uint32_t depth = 0) const;
		
		constexpr json_value() noexcept : value(0) {}
		constexpr json_value(json_value&& val) = default;
		json_value& operator=(json_value&& val) = default;
		constexpr json_value(const json_value& val) = default;
		json_value& operator=(const json_value& val) = default;
		~json_value() = default;
		// init as nullptr
		explicit constexpr json_value(std::nullptr_t) : value(nullptr) {}
		// init as object
		explicit json_value(const json_object& obj_) : value(obj_) {}
		explicit json_value(json_object&& obj_) : value(std::forward<json_object>(obj_)) {}
		// init as array
		explicit constexpr json_value(const json_array& arr_) : value(arr_) {}
		explicit constexpr json_value(json_array&& arr_) : value(std::forward<json_array>(arr_)) {}
		// init as string
		explicit constexpr json_value(const std::string& str_) : value(str_) {}
		explicit constexpr json_value(std::string&& str_) : value(std::forward<std::string>(str_)) {}
		explicit constexpr json_value(const char* str_) : value(str_) {}
		// init as signed integer
		explicit constexpr json_value(const int64_t& val) : value(val) {}
		// init as unsigned integer
		explicit constexpr json_value(const uint64_t& val) : value(std::bit_cast<int64_t>(val)) {}
		// init as single precision floating point
		explicit constexpr json_value(const float& val) : value(double(val)) {}
		// init as double precision floating point
		explicit constexpr json_value(const double& val) : value(val) {}
		// init as bool
		explicit constexpr json_value(const bool& flag_) : value(flag_) {}
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
		template <typename T> T get(const std::string& path,
									const T default_val = default_value<T>::def()) const;
		
		//! writes this document to disk as "filename",
		//! returns true on success, false otherwise
		bool write(const std::string& filename) const {
			std::ofstream file_stream(filename);
			if (!file_stream.is_open()) {
				return false;
			}
			print(file_stream);
			file_stream.close();
			return true;
		}
		
		//! dumps the document to the specified stream (defaults to "cout")
		void print(std::ostream& stream = std::cout) const {
			const auto cur_flags = stream.flags();
			const auto cur_precision = stream.precision();
			
			stream.setf(std::ios::dec | std::ios::showpoint);
			stream.precision(19);
			root.print(stream);
			
			stream.setf(cur_flags);
			stream.precision(cur_precision);
		}
	};
	
	//! reads the json file specified by 'filename' and creates a json document from it
	document create_document(const std::string& filename);
	
	//! creates a json document from the in-memory json data
	//! 'identifier' is used for error reporting/identification
	document create_document_from_string(const std::string& json_data, const std::string identifier = "");

} // namespace fl::json

namespace fl {

// explicit specializations of the json document getters
template<> struct json::document::default_value<std::string> {
	static std::string def() { return ""; }
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

#if !defined(FLOOR_JSON_EXTERN_TEMPLATE_INSTANTIATION)
extern template std::string json::document::get<std::string>(const std::string& path, const std::string default_value) const;
extern template float json::document::get<float>(const std::string& path, const float default_value) const;
extern template double json::document::get<double>(const std::string& path, const double default_value) const;
extern template uint64_t json::document::get<uint64_t>(const std::string& path, const uint64_t default_value) const;
extern template int64_t json::document::get<int64_t>(const std::string& path, const int64_t default_value) const;
extern template uint32_t json::document::get<uint32_t>(const std::string& path, const uint32_t default_value) const;
extern template int32_t json::document::get<int32_t>(const std::string& path, const int32_t default_value) const;
extern template bool json::document::get<bool>(const std::string& path, const bool default_value) const;
extern template json::json_object json::document::get<json::json_object>(const std::string& path, const json::json_object default_value) const;
extern template json::json_array json::document::get<json::json_array>(const std::string& path, const json::json_array default_value) const;
#endif

} // namespace fl
