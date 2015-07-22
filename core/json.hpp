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
		json_value root;
		bool valid { false };
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

#endif
