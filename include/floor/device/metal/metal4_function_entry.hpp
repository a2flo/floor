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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/device/device_function.hpp>
#include <floor/threading/resource_slot_handler.hpp>
#include <floor/device/metal/metal4_resource_tracking.hpp>

#if !defined(__OBJC__)
#error "only include this from Objective-C/C++ source files"
#endif

#include <Metal/MTL4ArgumentTable.h>

namespace fl {

class metal4_program;

//! used in exec_instance_t / argument tables for constant parameters that aren't backed by an individual user buffer
struct metal4_constant_buffer_info_t {
	uint32_t offset;
	uint32_t size;
	uint32_t buffer_idx;
};

class metal4_function_entry : public device_function::function_entry {
public:
	//! per execution instance data
	struct exec_instance_t {
		id <MTL4ArgumentTable> arg_table { nil };
		//! used to track all non-heap resources, which must be made resident before execution
		metal4_resource_tracking<true> resources {};
		//! contains the storage for all constants, nullptr if there are no constants
		std::shared_ptr<device_buffer> constants_buffer;
	};
	
protected:
	//! max possible concurrent execution instances
	static constexpr const uint32_t exec_instance_count { 16u };
	//! we have multiple execution instances so that the same function can be safely executed concurrently be multiple threads/users
	//! NOTE: only the first instance is created on init, all others are created on-demand at run-time (up to #exec_instance_count)
	mutable resource_slot_container<exec_instance_t, exec_instance_count> exec_instances;
	
public:
	MTL4LibraryFunctionDescriptor* function_descriptor { nil };
	MTL4ArgumentTableDescriptor* arg_table_descriptor { nil };
	id <MTLFunction> function { nil };
	id <MTLComputePipelineState> kernel_state { nil };
	bool supports_indirect_compute { false };
	
	//! acquires an execution instance
	//! NOTE: this may dynamically initialize an instance, performing this initializing within the specified "dev_queue"
	decltype(exec_instances)::manual_release_resource_t acquire_exec_instance(const device_queue& dev_queue) const;
	//! releases a previously acquired execution intsance again
	void release_exec_instance(const uint8_t slot_idx) const;
	
	//! argument index -> constant buffer info
	fl::flat_map<uint32_t, metal4_constant_buffer_info_t> constant_buffer_info;
	
	~metal4_function_entry() {
		@autoreleasepool {
			for (uint32_t res_idx = 0; res_idx < exec_instance_count; ++res_idx) {
				exec_instances.resources[res_idx].arg_table = nil;
			}
			kernel_state = nil;
			function_descriptor = nil;
			arg_table_descriptor = nil;
			function = nil;
		}
	}
	
protected:
	friend metal4_program;
	
	//! current amount of execution instances that have been created
	//! NOTE: this counter may go above "exec_instance_count", but we will still only allocate up to "exec_instance_count"
	mutable std::atomic<uint32_t> created_exec_instance_count { 0u };
	//! initializes/creates the execution instance at the specified resource/slot "idx"
	bool init_exec_instance(const device_queue& dev_queue, const uint32_t idx) const;
	
	//! align constants within the constants buffer to 16 bytes
	static constexpr const uint32_t constant_buffer_alignment { 16u };
	//! amount indiviudal constants in each constants buffer
	uint32_t constants_count { 0u };
	//! total amount in bytes of a constants buffer, including alignment
	uint32_t constant_buffer_size { 0u };
	
	//! fully initializes this metal4_function_entry, computing the required constants info and creating the first execution instance
	bool init(const device_queue& dev_queue);
	
};

} // namespace fl

#endif
