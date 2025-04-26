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

#include <floor/core/essentials.hpp>
#include <floor/threading/thread_safety.hpp>
#if defined(FLOOR_DEBUG)
#include <floor/core/logger.hpp>
#include <floor/threading/thread_helpers.hpp>
#endif
#include <floor/constexpr/ext_traits.hpp>
#include <bitset>
#include <thread>
#include <array>
#include <cassert>

namespace fl {

//! a thread-safe container of multiple resources of the same type, allowing thread-safe resource allocation/usage/release
template <typename resource_type, uint32_t resource_count, uint32_t stuck_count = 1000u>
class safe_resource_container {
public:
	//! true if "resource_type" is a smart ptr (shared_ptr/unique_ptr)
	static constexpr const bool is_smart_ptr_resource { ext::is_shared_ptr_v<resource_type> || ext::is_unique_ptr_v<resource_type> };
	//! specialized resource access type
	template <typename T> struct resource_access_type_handler { using type = T; };
	template <typename T> requires(is_smart_ptr_resource) struct resource_access_type_handler<T> { using type = typename resource_type::element_type*; };
	//! the type with which the resource is accessed
	using resource_access_type = typename resource_access_type_handler<resource_type>::type;
	
	explicit safe_resource_container(std::array<resource_type, resource_count>&& resources_) : resources(resources_) {}
	
	//! tries to acquire a resource,
	//! returns { resource, index } on success,
	//! returns { {}, ~0u } on failure
	std::pair<resource_access_type, uint32_t> try_acquire() REQUIRES(!resource_lock) {
		for (uint32_t trial = 0, limiter = 10; trial < limiter; ++trial) {
			{
				GUARD(resource_lock);
				if (!resources_in_use.all()) {
					for (uint32_t i = 0; i < resource_count; ++i) {
						if (!resources_in_use[i]) {
							resources_in_use.set(i);
							if constexpr (!is_smart_ptr_resource) {
								return { resources[i], i };
							} else {
								return { resources[i].get(), i };
							}
						}
					}
					floor_unreachable();
				}
			}
			std::this_thread::yield();
		}
		return { {}, ~0u };
	}
	
	//! acquires a resource, returns { resource, index }
	std::pair<resource_access_type, uint32_t> acquire() REQUIRES(!resource_lock) {
		for ([[maybe_unused]] uint32_t counter = 0;; ++counter) {
			{
				GUARD(resource_lock);
				if (!resources_in_use.all()) {
					for (uint32_t i = 0; i < resource_count; ++i) {
						if (!resources_in_use[i]) {
							resources_in_use.set(i);
							if constexpr (!is_smart_ptr_resource) {
								return { resources[i], i };
							} else {
								return { resources[i].get(), i };
							}
						}
					}
					floor_unreachable();
				}
			}
			std::this_thread::yield();
#if defined(FLOOR_DEBUG)
			if (counter == stuck_count) {
				log_warn("resource acquisition is probably stuck ($: $)", get_current_thread_name(), typeid(*this).name());
			}
#endif
		}
	}
	
	//! release a resource again (via resource)
	void release(const std::pair<resource_access_type, uint32_t>& resource) REQUIRES(!resource_lock) {
		GUARD(resource_lock);
		resources_in_use.reset(resource.second);
	}
	
	//! release a resource again (via index)
	void release(const uint32_t& resource_idx) REQUIRES(!resource_lock) {
		GUARD(resource_lock);
		resources_in_use.reset(resource_idx);
	}
	
protected:
	//! lock for "resources"
	safe_mutex resource_lock;
	//! contained resources
	std::array<resource_type, resource_count> resources GUARDED_BY(resource_lock) {};
	//! bitset of which resources are currently in use
	std::bitset<resource_count> resources_in_use GUARDED_BY(resource_lock) {};
	
};

} // namespace fl
