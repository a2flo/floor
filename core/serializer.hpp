/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2017 Florian Ziesche
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

#ifndef __FLOOR_SERIALIZER_HPP__
#define __FLOOR_SERIALIZER_HPP__

#include <floor/core/cpp_headers.hpp>

//! set this in-class with member variables that should be serializable
//! NOTE: the class must be constructible with the specified member variables, in the specified order
#define SERIALIZATION(class_type, ...) \
void serialize(serializer& s) { \
	s.serialize(__VA_ARGS__); \
} \
static class_type deserialize(serializer& s) { \
	typedef decltype(serializer::make_tuple_type_list(__VA_ARGS__)) tupled_arg_types; \
	return s.deserialize<class_type, tupled_arg_types>(make_index_sequence<tuple_size<tupled_arg_types>::value>()); \
} \
static constexpr bool is_serializable() { return true; } \
static constexpr bool is_serialization_size_static() { \
	typedef decltype(serializer::make_tuple_type_list(__VA_ARGS__)) tupled_arg_types; \
	constexpr const bool ret = serializer::is_serialization_size_static<tupled_arg_types>( \
		make_index_sequence<tuple_size<tupled_arg_types>::value>()); \
	return ret; \
} \
static constexpr size_t static_serialization_size() { \
	typedef decltype(serializer::make_tuple_type_list(__VA_ARGS__)) tupled_arg_types; \
	constexpr const size_t ret = serializer::static_serialization_size<tupled_arg_types>( \
		make_index_sequence<tuple_size<tupled_arg_types>::value>()); \
	return ret; \
}

class serializer {
protected:
	typedef vector<uint8_t> storage_type;
	storage_type data;
	
public:
	//! returns the raw data storage of this serializer
	storage_type& get_data() { return data; }
	const storage_type& get_data() const { return data; }
	
	//! serializes the specified parameters into this serializer data container
	template <typename type, typename... types>
	void serialize(const type& arg, const types&... args) {
		serialization<type>::serialize(data, arg);
		serialize(args...);
	}
	
	//! terminator
	void serialize() {}
	
	//! deserializes a "obj_type" class object from this serializer data container
	template <typename obj_type, typename tupled_arg_types, size_t... indices>
	obj_type deserialize(index_sequence<indices...>) {
		return { serialization<tuple_element_t<indices, tupled_arg_types>>::deserialize(data)... };
	}
	
	//! returns true if all specified types have a statically known size
	template <typename tupled_arg_types, size_t... indices>
	static constexpr bool is_serialization_size_static(index_sequence<indices...>) {
		constexpr const bool flags[] {
			serialization<tuple_element_t<indices, tupled_arg_types>>::is_size_static()...
		};
		bool ret = true;
		for(const auto& flag : flags) {
			ret &= flag;
		}
		return ret;
	}
	
	//! returns the statically known total size of the specified types, iff all are statically known, 0 otherwise
	template <typename tupled_arg_types, size_t... indices>
	static constexpr size_t static_serialization_size(index_sequence<indices...>) {
		constexpr const size_t sizes[] {
			serialization<tuple_element_t<indices, tupled_arg_types>>::static_size()...
		};
		size_t ret = 0;
		for(const auto& size : sizes) {
			if(size == 0) return 0;
			ret += size;
		}
		return ret;
	}
	
	//! helper function to create a tuple type consisting of the types of the specified variadic arguments
	template <typename... types>
	static constexpr auto make_tuple_type_list(types&&...) {
		return tuple<decay_t<types>...> {};
	}
	
protected:
	//! type-specific serialization implementation
	template <typename type, typename = void>
	struct serialization {
		static void serialize(storage_type&, const type&) {
			serialization_not_implemented_for_this_type<type>();
		}
		static type deserialize(storage_type&) {
			serialization_not_implemented_for_this_type<type>();
		}
		static constexpr bool is_size_static() { return false; }
		static constexpr size_t static_size() { return 0; }
	};
	
	//! user-friendly error if serialization has not been implemented for a type
	template <typename type>
	static void serialization_not_implemented_for_this_type()
	__attribute__((unavailable("serialization not implemented for this type")));
	
	//! helper wrapper to easily convert a uint64_t to 8 uint8_ts
	union uint64_as_bytes {
		uint64_t ui64;
		uint8_t bytes[8];
	};
	
	//! helper wrapper to easily convert a uint32_t to 4 uint8_ts
	union uint32_as_bytes {
		uint32_t ui32;
		uint8_t bytes[4];
	};
	
};

// handle erroneous cases
template <typename type> struct serializer::serialization<type, enable_if_t<is_pointer<type>::value>> {
	static void serialize(const type&)
	__attribute__((unavailable("serialization of pointers is not possible")));
	static type deserialize()
	__attribute__((unavailable("serialization of pointers is not possible")));
};

// integral and floating point types (+compiler extension int/fp types)
template <typename arith_type>
struct serializer::serialization<arith_type, enable_if_t<ext::is_arithmetic_v<arith_type>>> {
	static void serialize(storage_type& data, const arith_type& value) {
		uint8_t bytes[sizeof(arith_type)];
		memcpy(bytes, &value, sizeof(arith_type));
		data.insert(data.end(), begin(bytes), end(bytes));
	}
	static arith_type deserialize(storage_type& data) {
		arith_type ret;
		memcpy(&ret, data.data(), sizeof(arith_type));
		data.erase(data.begin(), data.begin() + sizeof(arith_type));
		return ret;
	}
	static constexpr bool is_size_static() { return true; }
	static constexpr size_t static_size() { return sizeof(arith_type); }
};

// enums
template <typename enum_type>
struct serializer::serialization<enum_type, enable_if_t<is_enum<enum_type>::value>> {
	static void serialize(storage_type& data, const enum_type& value) {
		uint8_t bytes[sizeof(enum_type)];
		memcpy(bytes, &value, sizeof(enum_type));
		data.insert(data.end(), begin(bytes), end(bytes));
	}
	static enum_type deserialize(storage_type& data) {
		enum_type ret;
		memcpy(&ret, data.data(), sizeof(enum_type));
		data.erase(data.begin(), data.begin() + sizeof(enum_type));
		return ret;
	}
	static constexpr bool is_size_static() { return true; }
	static constexpr size_t static_size() { return sizeof(enum_type); }
};

// floor vector types
template <typename vec_type>
struct serializer::serialization<vec_type, enable_if_t<is_floor_vector<vec_type>::value>> {
	typedef typename vec_type::this_scalar_type scalar_type;
	static_assert(!is_reference<scalar_type>::value, "can't serialize references");
	static_assert(!is_pointer<scalar_type>::value, "can't serialize pointers");
	
	static void serialize(storage_type& data, const vec_type& vec) {
		uint8_t bytes[sizeof(scalar_type) * vec_type::dim()];
#pragma unroll
		for(uint32_t i = 0; i < vec_type::dim(); ++i) {
			memcpy(bytes + i * sizeof(scalar_type), &vec[i], sizeof(scalar_type));
		}
		data.insert(data.end(), begin(bytes), end(bytes));
	}
	static vec_type deserialize(storage_type& data) {
		typedef typename vec_type::this_scalar_type scalar_type;
		vec_type ret;
#pragma unroll
		for(uint32_t i = 0; i < vec_type::dim(); ++i) {
			memcpy(&ret[i], data.data() + i * sizeof(scalar_type), sizeof(scalar_type));
		}
		data.erase(data.begin(), data.begin() + sizeof(scalar_type) * vec_type::dim());
		return ret;
	}
	static constexpr bool is_size_static() { return true; }
	static constexpr size_t static_size() { return sizeof(scalar_type) * vec_type::dim(); }
};

// string
template <> struct serializer::serialization<string> {
	static void serialize(storage_type& data, const string& str) {
		const uint64_as_bytes size { .ui64 = str.size() };
		data.insert(data.end(), begin(size.bytes), end(size.bytes));
		data.insert(data.end(), begin(str), end(str));
	}
	static string deserialize(storage_type& data) {
		uint64_t size = 0;
		memcpy(&size, data.data(), sizeof(size));
		
		string ret;
		ret.resize(size);
		memcpy(&ret[0], data.data() + sizeof(size), size);
		
		data.erase(data.begin(), data.begin() + sizeof(size) + string::difference_type(size));
		
		return ret;
	}
	static constexpr bool is_size_static() { return false; }
	static constexpr size_t static_size() { return 0; }
};

// vector
template <typename data_type> struct serializer::serialization<vector<data_type>> {
	static void serialize(storage_type& data, const vector<data_type>& vec) {
		const uint64_as_bytes size { .ui64 = vec.size() };
		data.insert(data.end(), begin(size.bytes), end(size.bytes));
		for(const auto& elem : vec) {
			serializer::serialization<data_type>::serialize(data, elem);
		}
	}
	static vector<data_type> deserialize(storage_type& data) {
		uint64_t size = 0;
		memcpy(&size, data.data(), sizeof(size));
		data.erase(data.begin(), data.begin() + sizeof(size));
		
		vector<data_type> ret;
		ret.resize(size);
		for(uint64_t i = 0; i < size; ++i) {
			ret[i] = serializer::serialization<data_type>::deserialize(data);
		}
		
		return ret;
	}
	static constexpr bool is_size_static() { return false; }
	static constexpr size_t static_size() { return 0; }
};

// array
template <typename data_type, size_t count> struct serializer::serialization<array<data_type, count>> {
	static void serialize(storage_type& data, const array<data_type, count>& arr) {
		for(const auto& elem : arr) {
			serializer::serialization<data_type>::serialize(data, elem);
		}
	}
	static array<data_type, count> deserialize(storage_type& data) {
		array<data_type, count> ret;
		for(uint32_t i = 0; i < count; ++i) {
			ret[i] = serializer::serialization<data_type>::deserialize(data);
		}
		return ret;
	}
	static constexpr bool is_size_static() { return serializer::serialization<data_type>::is_size_static(); }
	static constexpr size_t static_size() { return count * serializer::serialization<data_type>::static_size(); }
};

#endif
