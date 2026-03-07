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

#include <floor/device/metal/metal4_function_entry.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/device/device_context.hpp>
#include <floor/device/metal/metal4_buffer.hpp>
#include <floor/core/logger.hpp>

namespace fl {

bool metal4_function_entry::init(const device_queue& dev_queue) {
	using namespace toolchain;
	
	// handle implicit args which add to the total #args
	const auto is_soft_printf = has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(info->flags);
	const uint32_t implicit_arg_count = (is_soft_printf ? 1u : 0u);
	const uint32_t explicit_arg_count = uint32_t(info->args.size());
	const uint32_t total_arg_count = explicit_arg_count + implicit_arg_count;
	
	// find all top level constant arguments that we need to put into an implicit constants device buffer
	uint32_t buffer_count = 0u;
	uint32_t image_count = 0u;
	for (uint32_t i = 0; i < total_arg_count; ++i) {
		if (i < explicit_arg_count) {
			const auto& arg = info->args[i];
			
			if (has_flag<ARG_FLAG::ARGUMENT_BUFFER>(info->args[i].flags)) {
				// this is a single buffer, ignore all else
				++buffer_count;
				continue;
			}
			
			switch (arg.address_space) {
				case ARG_ADDRESS_SPACE::CONSTANT:
					if (arg.size == 0u || arg.size > 0xFFFF'FFFFu) {
						log_error("invalid constant argument size: $'", arg.size);
						return false;
					}
					
					// align for new entry + add it
					constant_buffer_size = (((constant_buffer_size + constant_buffer_alignment - 1u) / constant_buffer_alignment) *
											constant_buffer_alignment);
					constant_buffer_info.emplace(i, metal4_constant_buffer_info_t {
						.offset = constant_buffer_size,
						.size = uint32_t(arg.size),
						.buffer_idx = buffer_count,
					});
					constant_buffer_size += arg.size;
					++constants_count;
					++buffer_count;
					break;
				case ARG_ADDRESS_SPACE::GLOBAL:
					if (has_flag<ARG_FLAG::BUFFER_ARRAY>(arg.flags)) {
						buffer_count += uint32_t(arg.array_extent);
					} else {
						++buffer_count;
					}
					break;
				case ARG_ADDRESS_SPACE::IMAGE: {
					const uint32_t arg_images = (arg.access == ARG_ACCESS::READ_WRITE ? 2u : 1u);
					if (has_flag<ARG_FLAG::IMAGE_ARRAY>(arg.flags)) {
						image_count += uint32_t(arg.array_extent) * arg_images;
					} else {
						image_count += arg_images;
					}
					break;
				}
				case ARG_ADDRESS_SPACE::LOCAL:
					log_error("arg with a local address space is not supported (arg #$ in $)", i, info->name);
					return false;
				case ARG_ADDRESS_SPACE::UNKNOWN:
					if (has_flag<ARG_FLAG::STAGE_INPUT>(arg.flags)) {
						// ignore
						continue;
					}
					log_error("arg with an unknown address space (arg #$ in $)", i, info->name);
					return false;
			}
		} else {
			// implicit args
			if (i >= total_arg_count) {
				log_error("invalid arg count for function \"$\": $ is out-of-bounds", info->name, i);
				return false;
			}
			
			const auto implicit_arg_num = i - explicit_arg_count;
			if (is_soft_printf && implicit_arg_num == 0) {
				++buffer_count;
			} else {
				log_error("invalid implicit arg count for function \"$\": $ is out-of-bounds", info->name, implicit_arg_num);
				return false;
			}
		}
	}
	
	// final buffer size alignment
	constant_buffer_size = (((constant_buffer_size + constant_buffer_alignment - 1u) / constant_buffer_alignment) *
							constant_buffer_alignment);
	
	@autoreleasepool {
		// arg table setup
		arg_table_descriptor = [MTL4ArgumentTableDescriptor new];
		arg_table_descriptor.maxBufferBindCount = buffer_count;
		arg_table_descriptor.maxTextureBindCount = image_count;
		arg_table_descriptor.maxSamplerStateBindCount = 0u;
		arg_table_descriptor.initializeBindings = true;
		arg_table_descriptor.supportAttributeStrides = false;
#if defined(FLOOR_DEBUG)
		const auto debug_label = info->name + "_arg_table";
		arg_table_descriptor.label = [NSString stringWithUTF8String:debug_label.c_str()];
#endif
	}
	
	// only initialize the first execution instance here
	if (!init_exec_instance(dev_queue, 0u)) {
		return false;
	}
	created_exec_instance_count = 1u;
	
	// mark all instance from [1, max] as used and #0 as unused -> makes dynamic init thread-safe later on
	// NOTE: bits are flipped for this: 1 -> unused
	exec_instances.slots.slots = 1u;
	// NOTE: must change handling above if this ever changes
	static_assert(std::is_same_v<uint16_t, decltype(exec_instances.slots)::slot_bitset_t>);
	static_assert(exec_instance_count == 16u);
	
	return true;
}

bool metal4_function_entry::init_exec_instance(const device_queue& dev_queue, const uint32_t idx) const {
	const auto& mtl_dev = (const metal_device&)dev_queue.get_device();
	auto& res = exec_instances.resources[idx];
	
	@autoreleasepool {
		NSError* err = nil;
		res.arg_table = [mtl_dev.device newArgumentTableWithDescriptor:arg_table_descriptor error:&err];
		if (!res.arg_table || err) {
			log_error("failed to create argument table for function \"$\" instace #$: $",
					  info->name, idx, (err ? [[err localizedDescription] UTF8String] : "<no-error-msg>"));
			return false;
		}
		
#if defined(FLOOR_DEBUG)
		const auto idx_str = std::to_string(idx);
		const auto rs_debug_label = info->name + "_res_set#" + idx_str;
		res.resources.init(mtl_dev, rs_debug_label.c_str());
#else
		res.resources.init(mtl_dev);
#endif
		
		if (constants_count > 0) {
			assert(constant_buffer_size > 0u && (constant_buffer_size % constant_buffer_alignment) == 0u);
#if defined(FLOOR_DEBUG)
			const std::string constants_debug_label = info->name + "_constants#" + idx_str;
#endif
			res.constants_buffer = mtl_dev.context->create_buffer(dev_queue, constant_buffer_size,
																  MEMORY_FLAG::READ | MEMORY_FLAG::HOST_WRITE |
																  MEMORY_FLAG::HEAP_ALLOCATION
#if defined(FLOOR_DEBUG)
																  , constants_debug_label.c_str()
#endif
																  );
			
			// encode constants buffer address in argument table (only need to do this once)
			const auto base_address = ((metal4_buffer*)res.constants_buffer.get())->get_metal_buffer().gpuAddress;
			for (const auto&& const_entry : constant_buffer_info) {
				[res.arg_table setAddress:(base_address + const_entry.second.offset)
								  atIndex:const_entry.second.buffer_idx];
			}
		}
	}
	
	return true;
}

decltype(metal4_function_entry::exec_instances)::manual_release_resource_t
metal4_function_entry::acquire_exec_instance(const device_queue& dev_queue) const {
	if (auto res = exec_instances.try_acquire_resource_no_auto_release(); res) [[likely]] {
		return res;
	}
	
	// if we couldn't acquire an execution instance immediately, create a new instance as long as we're below the max limit
	while (created_exec_instance_count < exec_instance_count) [[unlikely]] {
		const auto slot_idx = created_exec_instance_count.fetch_add(1u);
		if (slot_idx < exec_instance_count) {
			if (!init_exec_instance(dev_queue, slot_idx)) {
				// very unfortunate, we will never be able to use this slot,
				// but the state is still safe as the slot is still marked as used
			} else {
				// success, claim the created slot / exec instance for us
				return { &exec_instances.resources[slot_idx], &exec_instances, uint8_t(slot_idx) };
			}
		}
		// else: we got unlucky and someone else got to init this slot before us
	}
	// once the max exec instance allocation count is reached, fall back to a normal spin-wait acquire
	return exec_instances.acquire_resource_no_auto_release();
}

void metal4_function_entry::release_exec_instance(const uint8_t slot_idx) const {
	exec_instances.release_resource(slot_idx);
}

} // namespace fl

#endif
