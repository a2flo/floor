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

#ifndef __FLOOR_FLAT_MAP_HPP__
#define __FLOOR_FLAT_MAP_HPP__

#include <functional>
#include <algorithm>
#include <vector>
using namespace std;

//! simple <key, value> map backed by a vector stored contiguously in memory (hence flat map),
//! technically O(n) lookup and insert, but usually faster than unordered_map or map for small maps
template <typename key_type, typename value_type> class flat_map {
public:
	//! single <key, value> entry in this map
	typedef pair<key_type, value_type> entry_type;
	
	//! map storage
	vector<entry_type> data;
	
	typedef typename decltype(data)::iterator iterator;
	typedef typename decltype(data)::const_iterator const_iterator;
	
	//! look up 'key' and if found, return its associated value, if not found,
	//! insert a new <key, value> pair using the value_types default constructor and return it
	value_type& operator[](const key_type& key) {
		const auto iter = find(key);
		if(iter != end(data)) {
			return iter->second;
		}
		return insert_or_assign(key, value_type {})->second;
	}
	
	//! returns a <found flag, iterator> pair, returning <true, iterator to <key, value>> if 'key' was found,
	//! and <false, end(data)> if 'key' was not found (will not modify this object)
	pair<bool, iterator> get(const key_type& key) {
		const auto iter = find(key);
		return { iter != end(data), iter };
	}
	
	//! returns a <found flag, const_iterator> pair, returning <true, const_iterator to <key, value>> if 'key' was found,
	//! and <false, end(data)> if 'key' was not found
	pair<bool, const_iterator> get(const key_type& key) const {
		const auto iter = find(key);
		return { iter != end(data), iter };
	}
	
	//! inserts a new <key, value> pair if no entry for 'key' exists yet, or replaces the current <key, value> entry if it does,
	//! returns an iterator to the <key, value> pair
	iterator insert_or_assign(const key_type& key, const value_type& value) {
		const auto iter = find(key);
		if(iter != end(data)) {
			iter->second = value;
			return *iter;
		}
		return data.emplace(end(data), key, value);
	}
	
	//! erases the <key, value> pair at 'iter',
	//! returns an iterator to the next entry
	iterator erase(iterator iter) {
		return data.erase(iter);
	}
	
	//! erases the <key, value> pairs from 'first' to 'last',
	//! returns an iterator to the next entry
	iterator erase(iterator first, iterator last) {
		return data.erase(first, last);
	}
	
	//! erases the <key, value> pair for the specified 'key', returns a <erased flag, iterator> pair set to
	//! <true, next entry> if 'key' was found, and <false, end(data)> if key was not found
	pair<bool, iterator> erase(const key_type& key) {
		const auto iter = find(key);
		if(iter != end(data)) {
			return { true, data.erase(iter) };
		}
		return { false, end(data) };
	}
	
	//! returns an iterator to the <key, value> pair corresponding to 'key', returns end(data) if not found
	iterator find(const key_type& key) {
		return find(begin(data), end(data), key);
	}
	
	//! returns a const_iterator to the <key, value> pair corresponding to 'key', returns end(data) if not found
	const_iterator find(const key_type& key) const {
		return find(begin(data), end(data), key);
	}
	
	//! returns 1 if a <key, value> entry for 'key' exists in this map, 0 if not
	size_t count(const key_type& key) const {
		return (find(key) != end(data) ? 1 : 0);
	}
	
	// forward auxiliary functions
	auto begin() { return begin(data); }
	auto end() { return end(data); }
	auto cbegin() const { return cbegin(data); }
	auto cend() const { return cend(data); }
	auto size() const { return size(data); }
	auto empty() const { return data.empty(); }
	auto clear() { data.clear(); }
	void reserve(const size_t& count) { data.reserve(count); }
	
};

#endif
