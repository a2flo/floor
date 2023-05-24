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

#ifndef __FLOOR_SERIALIZER_HPP__
#define __FLOOR_SERIALIZER_HPP__

#include <floor/core/cpp_headers.hpp>
#include <floor/constexpr/ext_traits.hpp>
#include <floor/math/vector_lib.hpp>

//! set this in-class with member variables that should be serializable
//! NOTE: the class must be constructible with the specified member variables, in the specified order
#define SERIALIZATION(class_type, ...) \
template <typename storage_type> \
void serialize(serializer<storage_type>& s) { \
	s.serialize(__VA_ARGS__); \
} \
template <typename storage_type> \
static class_type deserialize(serializer<storage_type>& s) { \
	typedef decltype(serializer<storage_type>::make_tuple_type_list(__VA_ARGS__)) tupled_arg_types; \
	return s.template deserialize<class_type, tupled_arg_types>(make_index_sequence<tuple_size<tupled_arg_types>::value>()); \
} \
template <typename storage_type> \
void deserialize_inplace(serializer<storage_type>& s) { \
	s.deserialize_inplace(__VA_ARGS__); \
} \
template <typename storage_type> \
static unique_ptr<class_type> deserialize_dynamic(serializer<storage_type>& s) { \
	auto obj = make_unique<class_type>(); \
	obj->deserialize_inplace(s); \
	return obj; \
} \
static constexpr bool is_serializable() { return true; } \
static constexpr bool is_serialization_size_static() { \
	typedef decltype(serializer<>::make_tuple_type_list(__VA_ARGS__)) tupled_arg_types; \
	constexpr const bool ret = serializer<>::is_serialization_size_static<tupled_arg_types>( \
		make_index_sequence<tuple_size<tupled_arg_types>::value>()); \
	return ret; \
} \
static constexpr size_t static_serialization_size() { \
	typedef decltype(serializer<>::make_tuple_type_list(__VA_ARGS__)) tupled_arg_types; \
	constexpr const size_t ret = serializer<>::static_serialization_size<tupled_arg_types>( \
		make_index_sequence<tuple_size<tupled_arg_types>::value>()); \
	return ret; \
} \
size_t serialization_size() const { \
	if constexpr(is_serialization_size_static()) { return static_serialization_size(); } \
	return serializer<>::serialization_size(__VA_ARGS__); \
}

//! serialization and deserialization of classes (class members)
//! "storage_type" must implement byte-wise .data(), .begin(), .end(), .insert(point, first, last), .erase(first, last)
template <typename storage_type = vector<uint8_t>>
class serializer {
protected:
	storage_type storage;
	
public:
	explicit serializer(storage_type&& storage_) : storage(std::forward<storage_type&&>(storage_)) {}
	
	//! returns the raw data storage of this serializer
	storage_type& get_storage() { return storage; }
	const storage_type& get_storage() const { return storage; }
	
	//! serializes the specified parameters into this serializer data container
	template <typename type, typename... types>
	void serialize(const type& arg, const types&... args) {
		serialization<type>::serialize(*this, storage, arg);
		if constexpr(sizeof...(args) > 0) {
			serialize(args...);
		}
	}
	
	//! since "is_constructible" is pretty much useless for this purpose, figure out ourselves if we can directly construct
	//! a type from its serialized members or if we also need to init/construct an empty base class
	template <typename obj_type = void>
	struct constructible_helper {
		template <typename, typename tag_type> using tag_select = tag_type;
		struct direct_constructor {};
		struct empty_base_constructor {};
		
		template <typename... arg_types>
		static constexpr auto constructor_type() -> tag_select<decltype(obj_type { arg_types{}... }), direct_constructor> { return {}; }
		
		template <typename... arg_types>
		static constexpr auto constructor_type() -> tag_select<decltype(obj_type { {}, arg_types{}... }), empty_base_constructor> { return {}; }
	};
	
	//! deserializes a "obj_type" class object from this serializer data container
	template <typename obj_type, typename tupled_arg_types, size_t... indices>
	obj_type deserialize(index_sequence<indices...>) {
		typedef decltype(constructible_helper<obj_type>::template constructor_type<tuple_element_t<indices, tupled_arg_types>...>()) constructor_type;
		constexpr const auto is_direct = is_same_v<constructor_type, typename constructible_helper<obj_type>::direct_constructor>;
		constexpr const auto is_empty_base = is_same_v<constructor_type, typename constructible_helper<obj_type>::empty_base_constructor>;
		static_assert(is_direct || is_empty_base,
					  "obj_type must be directly constructible or directly constructible with an empty base");
		
		if constexpr(is_direct) {
			return { serialization<tuple_element_t<indices, tupled_arg_types>>::deserialize(*this, storage)... };
		}
		else if constexpr(is_empty_base) {
			return { {}, serialization<tuple_element_t<indices, tupled_arg_types>>::deserialize(*this, storage)... };
		}
		return {};
	}
	
	//! in-place deserializes a "obj_type" class object from this serializer data container
	template <typename type, typename... types>
	void deserialize_inplace(type& arg, types&... args) {
		serialization<type>::deserialize_inplace(*this, storage, const_cast<remove_const_t<type>&>(arg));
		if constexpr(sizeof...(args) > 0) {
			deserialize_inplace(args...);
		}
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
	
	//! returns the size in bytes required to serialize the specified args / class
	template <typename type, typename... types>
	static size_t serialization_size(const type& arg, const types&... args) {
		size_t ret = serialization<type>::size(arg);
		if constexpr(sizeof...(args) > 0) {
			ret += serialization_size(args...);
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
	template <typename type>
	struct serialization {
		static void serialize(serializer&, storage_type&, const type&) {
			serialization_not_implemented_for_this_type<type>();
		}
		static type deserialize(serializer&, storage_type&) {
			serialization_not_implemented_for_this_type<type>();
		}
		static void deserialize_inplace(serializer&, storage_type&, type&) {
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
	
	// handle erroneous cases
	template <typename type> requires is_pointer_v<type>
	struct serialization<type> {
		static void serialize(serializer&, storage_type&, const type&)
		__attribute__((unavailable("serialization of pointers is not possible")));
		static type deserialize(serializer&, storage_type&)
		__attribute__((unavailable("serialization of pointers is not possible")));
		static void deserialize_inplace(serializer&, storage_type&, type&)
		__attribute__((unavailable("serialization of pointers is not possible")));
	};
	
	// integral and floating point types (+compiler extension int/fp types)
	template <typename arith_type> requires ext::is_arithmetic_v<arith_type>
	struct serialization<arith_type> {
		static void serialize(serializer&, storage_type& storage, const arith_type& value) {
			uint8_t bytes[sizeof(arith_type)];
			memcpy(bytes, &value, sizeof(arith_type));
			storage.insert(storage.end(), begin(bytes), end(bytes));
		}
		static arith_type deserialize(serializer&, storage_type& storage) {
			arith_type ret;
			memcpy(&ret, storage.data(), sizeof(arith_type));
			storage.erase(storage.begin(), storage.begin() + sizeof(arith_type));
			return ret;
		}
		static void deserialize_inplace(serializer&, storage_type& storage, arith_type& val) {
			memcpy(&val, storage.data(), sizeof(arith_type));
			storage.erase(storage.begin(), storage.begin() + sizeof(arith_type));
		}
		static constexpr bool is_size_static() { return true; }
		static constexpr size_t static_size() { return sizeof(arith_type); }
		static constexpr size_t size(const arith_type&) { return static_size(); }
	};
	
	// enums
	template <typename enum_type> requires is_enum_v<enum_type>
	struct serialization<enum_type> {
		static void serialize(serializer&, storage_type& storage, const enum_type& value) {
			uint8_t bytes[sizeof(enum_type)];
			memcpy(bytes, &value, sizeof(enum_type));
			storage.insert(storage.end(), begin(bytes), end(bytes));
		}
		static enum_type deserialize(serializer&, storage_type& storage) {
			enum_type ret;
			memcpy(&ret, storage.data(), sizeof(enum_type));
			storage.erase(storage.begin(), storage.begin() + sizeof(enum_type));
			return ret;
		}
		static void deserialize_inplace(serializer&, storage_type& storage, enum_type& val) {
			memcpy(&val, storage.data(), sizeof(enum_type));
			storage.erase(storage.begin(), storage.begin() + sizeof(enum_type));
		}
		static constexpr bool is_size_static() { return true; }
		static constexpr size_t static_size() { return sizeof(enum_type); }
		static constexpr size_t size(const enum_type&) { return static_size(); }
	};
	
	// floor vector types
	template <typename vec_type> requires is_floor_vector<vec_type>::value
	struct serialization<vec_type> {
		typedef typename vec_type::this_scalar_type scalar_type;
		static_assert(!is_reference<scalar_type>::value, "can't serialize references");
		static_assert(!is_pointer<scalar_type>::value, "can't serialize pointers");
		
		static void serialize(serializer&, storage_type& storage, const vec_type& vec) {
			uint8_t bytes[sizeof(scalar_type) * vec_type::dim()];
#pragma unroll
			for(uint32_t i = 0; i < vec_type::dim(); ++i) {
				memcpy(bytes + i * sizeof(scalar_type), &vec[i], sizeof(scalar_type));
			}
			storage.insert(storage.end(), begin(bytes), end(bytes));
		}
		static vec_type deserialize(serializer&, storage_type& storage) {
			vec_type ret;
#pragma unroll
			for(uint32_t i = 0; i < vec_type::dim(); ++i) {
				memcpy(&ret[i], storage.data() + i * sizeof(scalar_type), sizeof(scalar_type));
			}
			storage.erase(storage.begin(), storage.begin() + sizeof(scalar_type) * vec_type::dim());
			return ret;
		}
		static void deserialize_inplace(serializer&, storage_type& storage, vec_type& val) {
#pragma unroll
			for(uint32_t i = 0; i < vec_type::dim(); ++i) {
				memcpy(&val[i], storage.data() + i * sizeof(scalar_type), sizeof(scalar_type));
			}
			storage.erase(storage.begin(), storage.begin() + sizeof(scalar_type) * vec_type::dim());
		}
		static constexpr bool is_size_static() { return true; }
		static constexpr size_t static_size() { return sizeof(scalar_type) * vec_type::dim(); }
		static constexpr size_t size(const vec_type&) { return static_size(); }
	};
	
	// string
	template <typename char_type>
	struct serialization<basic_string<char_type>> {
		typedef basic_string<char_type> string_type;
		static void serialize(serializer&, storage_type& storage, const string_type& str) {
			const uint64_as_bytes size { .ui64 = str.size() * sizeof(char_type) };
			storage.insert(storage.end(), begin(size.bytes), end(size.bytes));
			storage.insert(storage.end(), begin(str), end(str));
		}
		static string_type deserialize(serializer&, storage_type& storage) {
			uint64_t size = 0;
			memcpy(&size, storage.data(), sizeof(size));
			
			string_type ret;
			ret.resize(size / sizeof(char_type));
			memcpy(&ret[0], storage.data() + sizeof(size), size);
			
			storage.erase(storage.begin(), storage.begin() + sizeof(size) + typename string_type::difference_type(size));
			
			return ret;
		}
		static void deserialize_inplace(serializer&, storage_type& storage, string_type& val) {
			uint64_t size = 0;
			memcpy(&size, storage.data(), sizeof(size));
			
			val.resize(size / sizeof(char_type));
			memcpy(&val[0], storage.data() + sizeof(size), size);
			
			storage.erase(storage.begin(), storage.begin() + sizeof(size) + typename string_type::difference_type(size));
		}
		static constexpr bool is_size_static() { return false; }
		static constexpr size_t static_size() { return 0; }
		static size_t size(const basic_string<char_type>& arg) {
			return sizeof(uint64_t) /* size */ + arg.size() * sizeof(char_type) /* data */;
		}
	};
	
	// vector
	template <typename data_type>
	struct serialization<vector<data_type>> {
		static void serialize(serializer& ser, storage_type& storage, const vector<data_type>& vec) {
			const uint64_as_bytes size { .ui64 = vec.size() };
			storage.insert(storage.end(), begin(size.bytes), end(size.bytes));
			for(const auto& elem : vec) {
				serializer::serialization<data_type>::serialize(ser, storage, elem);
			}
		}
		static vector<data_type> deserialize(serializer& ser, storage_type& storage) {
			uint64_t size = 0;
			memcpy(&size, storage.data(), sizeof(size));
			storage.erase(storage.begin(), storage.begin() + sizeof(size));
			
			vector<data_type> ret;
			ret.resize(size);
			for(uint64_t i = 0; i < size; ++i) {
				ret[i] = serializer::serialization<data_type>::deserialize(ser, storage);
			}
			
			return ret;
		}
		static void deserialize_inplace(serializer& ser, storage_type& storage, vector<data_type>& val) {
			uint64_t size = 0;
			memcpy(&size, storage.data(), sizeof(size));
			storage.erase(storage.begin(), storage.begin() + sizeof(size));
			
			val.resize(size);
			for(uint64_t i = 0; i < size; ++i) {
				serializer::serialization<data_type>::deserialize_inplace(ser, storage, val[i]);
			}
		}
		static constexpr bool is_size_static() { return false; }
		static constexpr size_t static_size() { return 0; }
		static size_t size(const vector<data_type>& arg) {
			if constexpr(serializer::serialization<data_type>::is_size_static()) {
				// if the stored data type size is static, we only need to multiply it with the dynamic count
				return sizeof(uint64_t) /* size */ + arg.size() * serializer::serialization<data_type>::static_size() /* data */;
			}
			else {
				size_t ret = 0;
				for(size_t i = 0, count = arg.size(); i < count; ++i) {
					ret += serializer::serialization<data_type>::size(arg[i]);
				}
				return sizeof(uint64_t) /* size */ + ret /* data */;
			}
		}
	};
	
	// array
	template <typename data_type, size_t count>
	struct serialization<array<data_type, count>> {
		static void serialize(serializer& ser, storage_type& storage, const array<data_type, count>& arr) {
			for(const auto& elem : arr) {
				serializer::serialization<data_type>::serialize(ser, storage, elem);
			}
		}
		static array<data_type, count> deserialize(serializer& ser, storage_type& storage) {
			array<data_type, count> ret;
			for(uint32_t i = 0; i < count; ++i) {
				ret[i] = serializer::serialization<data_type>::deserialize(ser, storage);
			}
			return ret;
		}
		static void deserialize_inplace(serializer& ser, storage_type& storage, array<data_type, count>& val) {
			for(uint32_t i = 0; i < count; ++i) {
				serializer::serialization<data_type>::deserialize_inplace(ser, storage, val[i]);
			}
		}
		static constexpr bool is_size_static() { return serializer::serialization<data_type>::is_size_static(); }
		static constexpr size_t static_size() { return count * serializer::serialization<data_type>::static_size(); }
		static size_t size(const array<data_type, count>& arg) {
			if constexpr(is_size_static()) {
				return static_size();
			}
			else {
				size_t ret = 0;
				for(size_t i = 0; i < count; ++i) {
					ret += serializer::serialization<data_type>::size(arg[i]);
				}
				return ret;
			}
		}
	};
	
	// serializable class
	template <typename ser_class_type> requires (ser_class_type::is_serializable())
	struct serialization<ser_class_type> {
		static void serialize(serializer& ser, storage_type&, const ser_class_type& obj) {
			obj.serialize(ser);
		}
		static ser_class_type deserialize(serializer& ser, storage_type&) {
			return ser_class_type::deserialize(ser);
		}
		static void deserialize_inplace(serializer& ser, storage_type&, ser_class_type& val) {
			val.deserialize_inplace(ser);
		}
		static constexpr bool is_size_static() { return ser_class_type::is_serialization_size_static(); }
		static constexpr size_t static_size() { return ser_class_type::static_serialization_size(); }
		static size_t size(const ser_class_type& obj) {
			if constexpr(is_size_static()) {
				return static_size();
			} else {
				return obj.serialization_size();
			}
		}
	};
	
};

#endif
