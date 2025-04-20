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

#include <floor/device/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/device/device_program.hpp>
#include <floor/device/toolchain.hpp>

namespace fl {

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class vulkan_device;
class vulkan_program final : public device_program {
public:
	//! stores a Vulkan program + function infos for an individual device
	struct vulkan_program_entry : program_entry {
		std::vector<VkShaderModule> programs;
		std::unordered_map<std::string, uint32_t> func_to_mod_map;
	};
	
	//! lookup map that contains the corresponding Vulkan program for multiple devices
	using program_map_type = fl::flat_map<const vulkan_device*, vulkan_program_entry>;
	
	vulkan_program(program_map_type&& programs);
	
	//! returns a static empty descriptor set layout for the specified device
	static VkDescriptorSetLayout get_empty_descriptor_set(const device& dev);
	
protected:
	const program_map_type programs;
	
};

FLOOR_POP_WARNINGS()

} // namespace fl

#endif
