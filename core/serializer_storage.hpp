/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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

#ifndef __FLOOR_SERIALIZER_STORAGE_HPP__
#define __FLOOR_SERIALIZER_STORAGE_HPP__

#include <floor/core/cpp_headers.hpp>
#include <floor/core/logger.hpp>

//! this can be used as an alternate storage to vector<uint8_t> within the serializer,
//! i.e. it is possible to have either read-only or write-only storage,
//! with read-only storage not actually erasing any data (-> faster)
template <bool can_read, bool can_write>
struct serializer_storage_wrapper {
	static_assert(can_read || can_write, "quite pointless");
	
	// const& if read-only, & if writable
	typedef conditional_t<can_write, vector<uint8_t>&, const vector<uint8_t>&> backing_storage_type;
	backing_storage_type backing_storage;
	const uint8_t* begin_ptr;
	const uint8_t* end_ptr;
	
	serializer_storage_wrapper(backing_storage_type backing_storage_,
							   const uint8_t* begin_ptr_,
							   const uint8_t* end_ptr_) noexcept :
	backing_storage(backing_storage_), begin_ptr(begin_ptr_), end_ptr(end_ptr_) {
		if(begin_ptr < &backing_storage[0] ||
		   begin_ptr > &backing_storage[backing_storage.empty() ? 0 : backing_storage.size() - 1]) {
			log_error("out-of-bounds begin_ptr");
			begin_ptr = backing_storage.data();
		}
		if(end_ptr < &backing_storage[0] || end_ptr > &backing_storage[backing_storage.size()] ||
		   end_ptr < begin_ptr) {
			log_error("out-of-bounds end_ptr");
			end_ptr = begin_ptr;
		}
	}
	
	const uint8_t* data() { return begin_ptr; }
	const uint8_t* data() const { return begin_ptr; }
	
	const uint8_t* begin() { return begin_ptr; }
	const uint8_t* begin() const { return begin_ptr; }
	
	const uint8_t* end() { return end_ptr; }
	const uint8_t* end() const { return end_ptr; }
	
	template <bool can_read_ = can_read, enable_if_t<can_read_>* = nullptr>
	const uint8_t* erase(const uint8_t* begin, const uint8_t* end) {
		if(begin == end) return nullptr;
		
		if(begin_ptr == end_ptr) {
			log_error("packet is empty");
			return nullptr;
		}
		
		if(end > end_ptr || begin >= end) {
			log_error("erasing past the end of the packet");
			return nullptr;
		}
		
		if(begin < begin_ptr || end < begin_ptr || begin > end) {
			log_error("invalid begin/end ptrs");
			return nullptr;
		}
		
		if(begin != begin_ptr) {
			log_error("can only erase from the start");
			return nullptr;
		}
		
		// "erase"
		begin_ptr = end;
		return begin_ptr;
	}
	
	template <typename iterator_type, bool can_write_ = can_write,
			  enable_if_t<can_write_ && !is_same<iterator_type, const uint8_t*>::value>* = nullptr>
	const uint8_t* insert(const uint8_t* insert_point,
						  iterator_type begin,
						  iterator_type end) {
		return insert(insert_point, (const uint8_t*)&*begin, (const uint8_t*)&*end);
	}
	
	template <bool can_write_ = can_write, enable_if_t<can_write_>* = nullptr>
	const uint8_t* insert(const uint8_t* insert_point, const uint8_t* begin, const uint8_t* end) {
		if(begin == end) return end_ptr;
		
		if(insert_point != end_ptr) {
			log_error("can only insert at the end");
			return nullptr;
		}
		
		if(begin > end) {
			log_error("invalid begin/end ptrs");
			return nullptr;
		}
		
		auto insert_iter = backing_storage.begin() + intptr_t(insert_point - begin_ptr);
		auto ret_ptr = backing_storage.insert(insert_iter, begin, end);
		
		// must invalidate begin/end
		begin_ptr = &backing_storage[0];
		end_ptr = &backing_storage[backing_storage.size()];
		
		return &*ret_ptr;
	}
	
};

typedef serializer_storage_wrapper<true, false> read_only_serializer_storage;
typedef serializer_storage_wrapper<false, true> write_only_serializer_storage;

#endif
