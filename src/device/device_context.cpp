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

#include <floor/device/device_context.hpp>
#include <floor/device/graphics_pipeline.hpp>
#include <floor/device/graphics_pass.hpp>
#include <floor/device/graphics_renderer.hpp>
#include <floor/device/indirect_command.hpp>

namespace fl {

const device* device_context::get_device(const device::TYPE type) const {
	switch(type) {
		case device::TYPE::ANY:
			// just return the first valid device if one exists
			return (!devices.empty() ? devices[0].get() : nullptr);
		case device::TYPE::FASTEST:
			if(fastest_device) return fastest_device;
			break;
		case device::TYPE::FASTEST_GPU:
			if(fastest_gpu_device) return fastest_gpu_device;
			break;
		case device::TYPE::FASTEST_CPU:
			if(fastest_cpu_device) return fastest_cpu_device;
			break;
		case device::TYPE::FASTEST_FLAG:
		case device::TYPE::NONE:
		case device::TYPE::ALL_CPU:
		case device::TYPE::ALL_GPU:
		case device::TYPE::ALL_DEVICES:
			log_warn("shouldn't use type $X to get a device!", type);
			return (!devices.empty() ? devices[0].get() : nullptr);
		default: break;
	}
	
	using enum_type = std::underlying_type_t<device::TYPE>;
	enum_type counter = 0;
	if(type >= device::TYPE::GPU0 && type <= device::TYPE::GPU255) {
		const enum_type num = (enum_type)type - (enum_type)device::TYPE::GPU0;
		for(const auto& dev : devices) {
			if(((enum_type)dev->type & (enum_type)device::TYPE::GPU) != 0) {
				if(num == counter) return dev.get();
				++counter;
			}
		}
	}
	else if(type >= device::TYPE::CPU0 && type <= device::TYPE::CPU255) {
		const enum_type num = (enum_type)type - (enum_type)device::TYPE::CPU0;
		for(const auto& dev : devices) {
			if(((enum_type)dev->type & (enum_type)device::TYPE::CPU) != 0) {
				if(num == counter) return dev.get();
				++counter;
			}
		}
	}
	// else: didn't find any or type is a weird mixture
	log_error("couldn't find a device matching the specified type $X, returning the first device instead!", type);
	return (!devices.empty() ? devices[0].get() : nullptr);
}

std::vector<const device*> device_context::get_devices() const {
	std::vector<const device*> ret;
	for (const auto& dev : devices) {
		ret.emplace_back(dev.get());
	}
	return ret;
}

const device* device_context::get_corresponding_device(const device& external_dev) const {
	for (const auto& dev : devices) {
		// check if the devices have UUIDs that can be compared
		if (dev->has_uuid && external_dev.has_uuid) {
			if (dev->uuid == external_dev.uuid) {
				return dev.get();
			}
		}
		// otherwise fall back to comparing vendor type and device name
		else {
			if (dev->vendor != external_dev.vendor) {
				continue;
			}
			if (dev->name != external_dev.name) {
				continue;
			}
			// found it
			return dev.get();
		}
	}
	return nullptr;
}

std::shared_ptr<device_buffer> device_context::wrap_buffer(const device_queue&, vulkan_buffer&, const MEMORY_FLAG) const {
	log_error("Vulkan buffer sharing is not supported by this backend");
	return {};
}

std::shared_ptr<device_image> device_context::wrap_image(const device_queue&, vulkan_image&, const MEMORY_FLAG) const {
	log_error("Vulkan image sharing is not supported by this backend");
	return {};
}

std::shared_ptr<device_buffer> device_context::wrap_buffer(const device_queue&, metal_buffer&, const MEMORY_FLAG) const {
	log_error("Metal buffer sharing is not supported by this backend");
	return {};
}

std::shared_ptr<device_image> device_context::wrap_image(const device_queue&, metal_image&, const MEMORY_FLAG) const {
	log_error("Metal image sharing is not supported by this backend");
	return {};
}

std::unique_ptr<indirect_command_pipeline> device_context::create_indirect_command_pipeline(const indirect_command_description& desc) const {
	auto pipeline = std::make_unique<generic_indirect_command_pipeline>(desc, devices);
	if (!pipeline || !pipeline->is_valid()) {
		return {};
	}
	return pipeline;
}

std::unique_ptr<graphics_pipeline> device_context::create_graphics_pipeline(const render_pipeline_description&, const bool) const {
	log_error("graphics not supported by this backend");
	return {};
}

std::unique_ptr<graphics_pass> device_context::create_graphics_pass(const render_pass_description&, const bool) const {
	log_error("graphics not supported by this backend");
	return {};
}

std::unique_ptr<graphics_renderer> device_context::create_graphics_renderer(const device_queue&, const graphics_pass&, const graphics_pipeline&,
																		const bool) const {
	log_error("graphics not supported by this backend");
	return {};
}

IMAGE_TYPE device_context::get_renderer_image_type() const {
	return IMAGE_TYPE::NONE;
}

uint4 device_context::get_renderer_image_dim() const {
	return {};
}

vr_context* device_context::get_renderer_vr_context() const {
	return nullptr;
}

void device_context::set_hdr_metadata(const hdr_metadata_t& hdr_metadata_) {
	hdr_metadata = hdr_metadata_;
}

const hdr_metadata_t& device_context::get_hdr_metadata() const {
	return hdr_metadata;
}

const float2& device_context::get_hdr_luminance_range() const {
	return hdr_metadata.luminance;
}

float device_context::get_hdr_range_max() const {
	return 1.0f;
}

float device_context::get_hdr_display_max_nits() const {
	return 80.0f;
}

void device_context::enable_resource_registry() {
	resource_registry_enabled = true;
}

std::weak_ptr<device_memory> device_context::get_memory_from_resource_registry(const std::string& label) {
	if (!resource_registry_enabled) {
		return {};
	}
	
	GUARD(resource_registry_lock);
	const auto iter = resource_registry.find(label);
	if (iter != resource_registry.end()) {
		return iter->second;
	}
	return {};
}

void device_context::update_resource_registry(const device_memory* ptr, const std::string& prev_label, const std::string& label) {
	if (!resource_registry_enabled) {
		return;
	}
	
	GUARD(resource_registry_lock);
	const auto ptr_iter = resource_registry_ptr_lut.find(ptr);
	if (ptr_iter == resource_registry_ptr_lut.end()) {
		// not registered, don't update anything
		return;
	}
	
	// always update reverse LUT
	resource_registry_reverse.insert_or_assign(ptr, label);
	
	// update registry:
	//  * if the label is identical, this is a nop
	//  * if the previous label is non-empty, remove it from the registry
	//  * insert/assign new registry entry for the new label and resource
	if (prev_label == label) {
		return;
	}
	if (!prev_label.empty()) {
		resource_registry.erase(prev_label);
	}
	resource_registry.insert_or_assign(label, ptr_iter->second);
}

void device_context::remove_from_resource_registry(const device_memory* ptr) {
	if (!resource_registry_enabled) {
		return;
	}
	
	GUARD(resource_registry_lock);
	const auto label_iter = resource_registry_reverse.find(ptr);
	if (label_iter != resource_registry_reverse.end()) {
		if (!label_iter->second.empty()) {
			resource_registry.erase(label_iter->second);
		}
	}
	resource_registry_reverse.erase(ptr);
	resource_registry_ptr_lut.erase(ptr);
}

std::vector<std::string> device_context::get_resource_registry_keys() const REQUIRES(!resource_registry_lock) {
	if (!resource_registry_enabled) {
		return {};
	}
	
	GUARD(resource_registry_lock);
	std::vector<std::string> ret;
	ret.reserve(resource_registry.size());
	for (const auto& entry : resource_registry) {
		ret.emplace_back(entry.first);
	}
	return ret;
}

std::vector<std::weak_ptr<device_memory>> device_context::get_resource_registry_weak_resources() const REQUIRES(!resource_registry_lock) {
	if (!resource_registry_enabled) {
		return {};
	}
	
	GUARD(resource_registry_lock);
	std::vector<std::weak_ptr<device_memory>> ret;
	ret.reserve(resource_registry_ptr_lut.size());
	for (const auto& entry : resource_registry_ptr_lut) {
		ret.emplace_back(entry.second);
	}
	return ret;
}

std::vector<std::shared_ptr<device_queue>> device_context::create_distinct_queues(const device& dev, const uint32_t wanted_count) const {
	if (wanted_count == 0) {
		return {};
	}
	
	std::vector<std::shared_ptr<device_queue>> ret;
	ret.reserve(wanted_count);
	for (uint32_t i = 0; i < wanted_count; ++i) {
		ret.emplace_back(create_queue(dev));
	}
	return ret;
}

std::vector<std::shared_ptr<device_queue>> device_context::create_distinct_compute_queues(const device& dev, const uint32_t wanted_count) const {
	if (wanted_count == 0) {
		return {};
	}
	
	std::vector<std::shared_ptr<device_queue>> ret;
	ret.reserve(wanted_count);
	for (uint32_t i = 0; i < wanted_count; ++i) {
		ret.emplace_back(create_compute_queue(dev));
	}
	return ret;
}

} // namespace fl
