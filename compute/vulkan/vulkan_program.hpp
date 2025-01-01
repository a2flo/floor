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

#include <floor/compute/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/compute/compute_program.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/compute/vulkan/vulkan_descriptor_set.hpp>

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class vulkan_device;
class vulkan_program final : public compute_program {
public:
	//! stores a vulkan program + function infos for an individual device
	struct vulkan_program_entry : program_entry {
		vector<VkShaderModule> programs;
		unordered_map<string, uint32_t> func_to_mod_map;
	};
	
	//! lookup map that contains the corresponding vulkan program for multiple devices
	typedef floor_core::flat_map<const vulkan_device&, vulkan_program_entry> program_map_type;
	
	vulkan_program(program_map_type&& programs);
	
	static optional<vulkan_descriptor_set_layout_t> build_descriptor_set_layout(const compute_device& dev,
																				const string& func_name,
																				const llvm_toolchain::function_info& info,
																				const VkShaderStageFlagBits stage);
	
	//! returns a static empty descriptor set layout for the specified device
	static VkDescriptorSetLayout get_empty_descriptor_set(const compute_device& dev);
	
protected:
	const program_map_type programs;
	
};

FLOOR_POP_WARNINGS()

#endif
