/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#include <Metal/MTLResource.h>

//! helper class to store all used Metal resources, with specific uses
//! NOTE: used by Metal argument buffer and indirect commands
class metal_resource_tracking {
public:
	metal_resource_tracking() = default;
	
	//! contains the state of multiple/all tracked resources
	//! TODO/NOTE: right now, all buffers are considered read+write, images may be read-only or read+write (TODO: handle write-only/read-only buffers)
	struct resource_info_t {
		vector<id <MTLResource> __unsafe_unretained> read_only;
		vector<id <MTLResource> __unsafe_unretained> write_only;
		vector<id <MTLResource> __unsafe_unretained> read_write;
		vector<id <MTLResource> __unsafe_unretained> read_only_images;
		vector<id <MTLResource> __unsafe_unretained> read_write_images;
		
		//! adds resources from another resource tracking object state
		void add_resources(const resource_info_t& other) {
			read_only.insert(read_only.end(), other.read_only.begin(), other.read_only.end());
			write_only.insert(write_only.end(), other.write_only.begin(), other.write_only.end());
			read_write.insert(read_write.end(), other.read_write.begin(), other.read_write.end());
			read_only_images.insert(read_only_images.end(), other.read_only_images.begin(), other.read_only_images.end());
			read_write_images.insert(read_write_images.end(), other.read_write_images.begin(), other.read_write_images.end());
		}
	};
	
	//! returns the currently tracked resources
	const resource_info_t& get_resources() const {
		return resources;
	}
	
	//! clears all currently tracked resources
	void clear_resources() {
		resources = {};
	}
	
	//! adds resources from another resource tracking object
	void add_resources(const resource_info_t& other) {
		resources.add_resources(other);
	}
	
protected:
	//! currently tracked resources
	resource_info_t resources;
	
	//! helper function to sort the specified resources and unique them, as well as kill nil resources
	static void sort_and_unique_resources(vector<id <MTLResource> __unsafe_unretained>& res) {
		std::sort(res.begin(), res.end());
		auto last_iter = std::unique(res.begin(), res.end());
		if (last_iter != res.end()) {
			res.erase(last_iter, res.end());
		}
		// kill nil resources
		for (auto iter = res.begin(); iter != res.end(); ) {
			if (*iter == nil) {
				iter = res.erase(iter);
			} else {
				++iter;
			}
		}
	}
	
	//! sorts + uniques all resources
	void sort_and_unique_all_resources() {
		sort_and_unique_resources(resources.read_only);
		sort_and_unique_resources(resources.write_only);
		sort_and_unique_resources(resources.read_write);
		sort_and_unique_resources(resources.read_only_images);
		sort_and_unique_resources(resources.read_write_images);
	}
};

#endif
