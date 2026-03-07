/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#if !defined(FLOOR_NO_METAL)

#include <Metal/MTLAllocation.h>
#include <Metal/MTLResource.h>
#include <Metal/MTLResidencySet.h>
#include <floor/device/metal/metal_device.hpp>

namespace fl {

template <bool with_residency_set> class metal4_resource_tracking;

//! contains the state of multiple/all tracked resources
struct metal4_resource_container_t {
	mutable bool is_dirty { false };
	
#if defined(FLOOR_DEBUG)
	void validate_allocation(id <MTLAllocation> __unsafe_unretained res) {
		@autoreleasepool {
			// must not be nil
			assert(res != nil);
			// must not be heap allocated
			if ([res conformsToProtocol:@protocol(MTLResource)]) {
				id <MTLResource> __unsafe_unretained res_ = (id <MTLResource>)res;
				assert([res_ heap] == nil);
			}
		}
	}
#endif
	
	//! adds a resource to the tracking
	void add_resource(id <MTLAllocation> __unsafe_unretained res) {
		@autoreleasepool {
#if defined(FLOOR_DEBUG)
			validate_allocation(res);
#endif
			resources.emplace_back(res);
			is_dirty = true;
		}
	}
	
	//! adds resources to the tracking
	void add_resources(std::vector<id <MTLAllocation> __unsafe_unretained>& add) {
		@autoreleasepool {
			if (!add.empty()) {
#if defined(FLOOR_DEBUG)
				for (const auto& res : add) {
					validate_allocation(res);
				}
#endif
				resources.insert(resources.end(), add.begin(), add.end());
				is_dirty = true;
			}
		}
	}
	
	//! adds resources from another resource tracking object state
	void add_resources(const metal4_resource_container_t& other) {
		@autoreleasepool {
			if (!other.resources.empty()) {
#if defined(FLOOR_DEBUG)
				for (const auto& res : other.resources) {
					validate_allocation(res);
				}
#endif
				resources.insert(resources.end(), other.resources.begin(), other.resources.end());
				is_dirty = true;
			}
		}
	}
	
	//! returns true if any resources are currently tracked
	bool has_resources() const {
		return !resources.empty();
	}
	
	//! clears all currently tracked resources
	void clear() {
		@autoreleasepool {
			if (!resources.empty()) {
				resources.clear();
				is_dirty = true;
			}
		}
	}
	
	//! sorts the resources and uniques them, as well as kill nil resources
	void sort_and_unique_resources() {
		@autoreleasepool {
			if (resources.empty()) {
				return;
			}
			
			std::sort(resources.begin(), resources.end());
			auto last_iter = std::unique(resources.begin(), resources.end());
			if (last_iter != resources.end()) {
				resources.erase(last_iter, resources.end());
			}
			
			// kill nil resources
			for (auto iter = resources.begin(); iter != resources.end(); ) {
				if (*iter == nil) {
					iter = resources.erase(iter);
				} else {
					++iter;
				}
			}
			
			is_dirty = true;
		}
	}
	
protected:
	friend metal4_resource_tracking<true>;
	mutable std::vector<id <MTLAllocation> __unsafe_unretained> resources;
	
};

//! helper class to store/track all used Metal resources
template <bool with_residency_set = true>
class metal4_resource_tracking {
public:
	//! deferred init constructor -> must call init() afterwards
	constexpr metal4_resource_tracking() noexcept = default;
	
	//! direct init constructor
	metal4_resource_tracking(const metal_device& mtl_dev_, const char* debug_label = nullptr) : mtl_dev(&mtl_dev_) {
		@autoreleasepool {
			internal_init(debug_label);
		}
	}
	metal4_resource_tracking(const metal4_resource_tracking&) = default;
	metal4_resource_tracking& operator=(const metal4_resource_tracking&) = default;
	
	~metal4_resource_tracking() {
		@autoreleasepool {
			resource_container = {};
			residency_set = nullptr;
		}
	}
	
	//! must only be called when using the deferred init constructor
	void init(const metal_device& mtl_dev_, const char* debug_label = nullptr) {
		mtl_dev = &mtl_dev_;
		internal_init(debug_label);
	}
	
	//! returns true if any resources are currently tracked
	bool has_resources() const {
		return resource_container.has_resources();
	}
	
	//! returns the currently tracked resources
	metal4_resource_container_t& get_resources() {
		return resource_container;
	}
	
	//! returns the currently tracked resources
	const metal4_resource_container_t& get_resources() const {
		return resource_container;
	}
	
	//! clears all currently tracked resources
	void clear_resources() {
		@autoreleasepool {
			resource_container.clear();
		}
	}
	
	//! adds resources from another resource tracking object
	void add_resources(const metal4_resource_container_t& other) {
		resource_container.add_resources(other);
	}
	
	//! updates all resources in the residency set (if dirty) and requests them to be made resident
	//! NOTE: this generally seems to be counterproductive, especially when used too often
	void request_residency() const requires (with_residency_set) {
		@autoreleasepool {
			update_and_commit();
			[residency_set requestResidency];
		}
	}
	
	//! requests for all resources to be made non-resident
	//! NOTE: for resources that are also tracked/resident in other residency sets, this has no effect
	void end_residency() const requires (with_residency_set) {
		[residency_set endResidency];
	}
	
	//! if dirty: updates all resources in the residency set + commits them
	void update_and_commit() const requires (with_residency_set) {
		if (!resource_container.is_dirty) {
			return;
		}
		
		@autoreleasepool {
			[residency_set removeAllAllocations];
			
			if (!resource_container.resources.empty()) {
				[residency_set addAllocations:resource_container.resources.data()
										count:resource_container.resources.size()];
			}
			
			[residency_set commit];
			resource_container.is_dirty = false;
		}
	}
	
	//! returns the associated Metal device (if properly initialized)
	const metal_device* get_device() const {
		return mtl_dev;
	}
	
protected:
	//! currently tracked resources
	metal4_resource_container_t resource_container;
	const metal_device* mtl_dev { nullptr };
	
	void internal_init(const char* debug_label floor_unused_if_release) {
		if constexpr (with_residency_set) {
			@autoreleasepool {
				MTLResidencySetDescriptor* desc = [MTLResidencySetDescriptor new];
#if defined(FLOOR_DEBUG)
				if (debug_label) {
					desc.label = [NSString stringWithUTF8String:debug_label];
				}
#endif
				NSError* err = nil;
				residency_set = [mtl_dev->device newResidencySetWithDescriptor:desc
																		 error:&err];
				if (!residency_set || err) {
					throw std::runtime_error("failed to create residency set for device " + mtl_dev->name + ": " +
											 (err ? [[err localizedDescription] UTF8String] : "<no-error-msg>"));
				}
			}
		}
	}
	
	//! sorts + uniques all resources
	void sort_and_unique_all_resources() {
		resource_container.sort_and_unique_resources();
	}
	
public:
	//! residency set containing the resources/allocations
	std::conditional_t<with_residency_set, id <MTLResidencySet>, void*> residency_set { nullptr };
	
};

} // namespace fl

#endif
