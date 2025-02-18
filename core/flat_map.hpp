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

#if (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 200100)
#include <flat_map>
#else
#include <functional>
#include <algorithm>
#include <vector>
#include <exception>
#include <string>
using namespace std;
#endif

namespace floor_core {

// make use of std::flat_map when available (libc++ 20.1+)
#if (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 200100)
template <typename key_type, typename value_type> using flat_map = std::flat_map<key_type, value_type>;
#else // otherwise use our implementation
//! simple <key, value> map backed by a vector stored contiguously in memory (hence flat map),
//! technically O(n) lookup and insert, but usually faster than unordered_map or map for small maps
template <typename key_type, typename value_type> class flat_map {
public:
	//! returns true if the key is a reference
	static constexpr bool is_ref_key() {
		return is_lvalue_reference_v<key_type>;
	}
	
	//! if key_type is a reference, we can't actually store it -> use a reference_wrapper for it,
	//! otherwise: storage_key_type == key_type
	using storage_key_type = conditional_t<is_ref_key(), reference_wrapper<remove_reference_t<key_type>>, key_type>;
	
	//! single <key, value> entry in this map
	using entry_type = pair<storage_key_type, value_type>;
	
protected:
	//! map storage
	vector<entry_type> data;
	
	//! removes all duplicate entries for each unique key in this map
	void unique() {
		const auto end_iter = unique(begin(), end(), [](const entry_type& kv_1, const entry_type& kv_2) {
			return (kv_1.first == kv_2.first);
		});
		data.erase(end_iter, end());
	}
	
	//! helper function to get the underlying key type from the storage_key_type
	static const key_type& get_key(const storage_key_type& skey) {
		if constexpr (is_ref_key()) {
			return skey.get();
		} else {
			return skey;
		}
	}
	
	//! helper function to get the underlying key type from the storage_key_type
	static key_type& get_key(storage_key_type& skey) {
		if constexpr (is_ref_key()) {
			return skey.get();
		} else {
			return skey;
		}
	}
	
public:
	typedef typename decltype(data)::iterator iterator;
	typedef typename decltype(data)::const_iterator const_iterator;
	
	//! default empty map constructor
	constexpr flat_map() noexcept = default;
	
	//! move construct from another flat_map
	flat_map(flat_map&& fmap) : data(std::move(fmap.data)) {}
	
	//! copy construct from another flat_map
	flat_map(const flat_map& fmap) : data(fmap.data) {}
	
	//! move assignment from another flat_map
	flat_map& operator=(flat_map&& fmap) {
		data = std::move(fmap.data);
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
	flat_map(vector<entry_type>&& vec) : data(std::move(vec)) {
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
	
	//! look up 'key' and if found, return its associated value,
	//! if not found, throws std::out_of_range
	value_type& at(const key_type& key) {
		const auto iter = find(key);
		if (iter == end()) {
			if constexpr (is_same_v<key_type, string> || is_same_v<key_type, string_view>) {
				throw out_of_range("key not found: " + string(key));
			} else {
				throw out_of_range("key not found");
			}
		}
		return iter->second;
	}
	
	//! look up 'key' and if found, return its associated value,
	//! if not found, throws std::out_of_range
	const value_type& at(const key_type& key) const {
		const auto iter = find(key);
		if (iter == end()) {
			if constexpr (is_same_v<key_type, string> || is_same_v<key_type, string_view>) {
				throw out_of_range("key not found: " + string(key));
			} else {
				throw out_of_range("key not found");
			}
		}
		return iter->second;
	}
	
	//! inserts a new <key, value> pair if no entry for 'key' exists yet, or replaces the current <key, value> entry if it does,
	//! returns an iterator to the <key, value> pair, as well as a bool signaling whether insertion (true) or assignment (false) took place
	pair<iterator, bool> insert_or_assign(const key_type& key, const value_type& value) {
		auto iter = find(key);
		if(iter != end()) {
			iter->second = value;
			return { iter, false };
		}
		return { data.emplace(end(), key, value), true };
	}
	
	//! inserts a new <key, value> pair if no entry for 'key' exists yet, or replaces the current <key, value> entry if it does,
	//! returns an iterator to the <key, value> pair, as well as a bool signaling whether insertion (true) or assignment (false) took place
	pair<iterator, bool> insert_or_assign(const key_type& key, value_type&& value) {
		auto iter = find(key);
		if (iter != end()) {
			iter->second = std::move(value);
			return { iter, false };
		}
		return { data.emplace(end(), key, std::forward<value_type>(value)), true };
	}
	
	//! inserts a new <key, value> pair if no entry for 'key' exists yet and returns pair<iterator to it, true>,
	//! or returns pair<iterator to the current <key, value> entry, false> if it already exists
	pair<iterator, bool> insert(const key_type& key, const value_type& value) {
		auto iter = find(key);
		if(iter != end()) {
			return { iter, false };
		}
		return { data.emplace(end(), key, value), true };
	}
	
	//! inserts a new <key, value> pair if no entry for 'key' exists yet and returns pair<iterator to it, true>,
	//! or returns pair<iterator to the current <key, value> entry, false> if it already exists
	template <typename empl_key_type, typename empl_value_type>
	pair<iterator, bool> emplace(empl_key_type&& key, empl_value_type&& value) {
		auto iter = find(key);
		if(iter != end()) {
			return { iter, false };
		}
		return { data.emplace(end(), std::move(key), std::move(value)), true };
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
	
	//! erases the <key, value> pair for the specified 'key', returns the number of erased <key, value> entries
	size_t erase(const key_type& key) {
		const auto iter = find(key);
		if (iter != end()) {
			data.erase(iter);
			return 1uz;
		}
		return 0uz;
	}
	
	//! returns an iterator to the <key, value> pair corresponding to 'key', returns end() if not found
	iterator find(const key_type& key) {
		return find_if(data.begin(), data.end(), [&key](const entry_type& entry) {
			return (get_key(entry.first) == key);
		});
	}
	
	//! returns a const_iterator to the <key, value> pair corresponding to 'key', returns end() if not found
	const_iterator find(const key_type& key) const {
		return find_if(data.cbegin(), data.cend(), [&key](const entry_type& entry) {
			return (get_key(entry.first) == key);
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
	auto rbegin() { return data.rbegin(); }
	auto rend() { return data.rend(); }
	auto rbegin() const { return data.rbegin(); }
	auto rend() const { return data.rend(); }
	auto crbegin() const { return data.crbegin(); }
	auto crend() const { return data.crend(); }
	auto size() const { return data.size(); }
	auto empty() const { return data.empty(); }
	auto clear() { data.clear(); }
	
};
#endif

} // namespace floor_core
