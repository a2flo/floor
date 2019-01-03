/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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
	
protected:
	//! map storage
	vector<entry_type> data;
	
	//! removes all duplicate entries for each unique key in this map
	void unique() {
		const auto end_iter = unique(begin(), end(), [](const key_type& kv_1, const value_type& kv_2) {
			return (kv_1.first == kv_2.first);
		});
		data.erase(end_iter, end());
	}
	
public:
	typedef typename decltype(data)::iterator iterator;
	typedef typename decltype(data)::const_iterator const_iterator;
	
	//! default empty map constructor
	constexpr flat_map() noexcept {}
	
	//! move construct from another flat_map
	flat_map(flat_map&& fmap) : data(move(fmap.data)) {}
	
	//! copy construct from another flat_map
	flat_map(const flat_map& fmap) : data(fmap.data) {}
	
	//! move assignment from another flat_map
	flat_map& operator=(flat_map&& fmap) {
		data = move(fmap.data);
		return *this;
	}
	
	//! copy assignment from another flat_map
	flat_map& operator=(const flat_map& fmap) {
		data = fmap.data;
		return *this;
	}
	
	//! construct through a initializer_list, note that all entries will be uniqued
	flat_map(initializer_list<entry_type> ilist) : data(ilist) {
		unique();
	}
	
	//! move construct through a vector, note that all entries will be uniqued
	flat_map(vector<entry_type>&& vec) : data(move(vec)) {
		unique();
	}
	
	//! copy construct through a vector, note that all entries will be uniqued
	flat_map(const vector<entry_type>& vec) : data(vec) {
		unique();
	}
	
	//! look up 'key' and if found, return its associated value, if not found,
	//! insert a new <key, value> pair using the value_types default constructor and return it
	value_type& operator[](const key_type& key) {
		const auto iter = find(key);
		if(iter != end()) {
			return iter->second;
		}
		return insert_or_assign(key, value_type {})->second;
	}
	
	//! returns a <found flag, iterator> pair, returning <true, iterator to <key, value>> if 'key' was found,
	//! and <false, end()> if 'key' was not found (will not modify this object)
	pair<bool, iterator> get(const key_type& key) {
		const auto iter = find(key);
		return { iter != end(), iter };
	}
	
	//! returns a <found flag, const_iterator> pair, returning <true, const_iterator to <key, value>> if 'key' was found,
	//! and <false, end()> if 'key' was not found
	pair<bool, const_iterator> get(const key_type& key) const {
		const auto iter = find(key);
		return { iter != end(), iter };
	}
	
	//! inserts a new <key, value> pair if no entry for 'key' exists yet, or replaces the current <key, value> entry if it does,
	//! returns an iterator to the <key, value> pair
	iterator insert_or_assign(const key_type& key, const value_type& value) {
		auto iter = find(key);
		if(iter != end()) {
			iter->second = value;
			return iter;
		}
		return data.emplace(end(), key, value);
	}
	
	//! inserts a new <key, value> pair if no entry for 'key' exists yet and returns pair<true, iterator to it>,
	//! or returns pair<false, iterator to the current <key, value> entry> if it already exists
	pair<bool, iterator> insert(const key_type& key, const value_type& value) {
		auto iter = find(key);
		if(iter != end()) {
			return { false, iter };
		}
		return { true, data.emplace(end(), key, value) };
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
	//! <true, next entry> if 'key' was found, and <false, end()> if key was not found
	pair<bool, iterator> erase(const key_type& key) {
		const auto iter = find(key);
		if(iter != end()) {
			return { true, data.erase(iter) };
		}
		return { false, end() };
	}
	
	//! returns an iterator to the <key, value> pair corresponding to 'key', returns end() if not found
	iterator find(const key_type& key) {
		return find_if(data.begin(), data.end(), [&key](const entry_type& entry) {
			return (entry.first == key);
		});
	}
	
	//! returns a const_iterator to the <key, value> pair corresponding to 'key', returns end() if not found
	const_iterator find(const key_type& key) const {
		return find_if(data.cbegin(), data.cend(), [&key](const entry_type& entry) {
			return (entry.first == key);
		});
	}
	
	//! returns 1 if a <key, value> entry for 'key' exists in this map, 0 if not
	size_t count(const key_type& key) const {
		return (find(key) != end() ? 1 : 0);
	}
	
	// forward auxiliary functions
	auto begin() { return data.begin(); }
	auto end() { return data.end(); }
	auto begin() const { return data.begin(); }
	auto end() const { return data.end(); }
	auto cbegin() const { return data.cbegin(); }
	auto cend() const { return data.cend(); }
	auto size() const { return data.size(); }
	auto empty() const { return data.empty(); }
	auto clear() { data.clear(); }
	void reserve(const size_t& count) { data.reserve(count); }
	
};

#endif
