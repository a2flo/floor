/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#include <floor/compute/compute_context.hpp>
#include <floor/graphics/graphics_pipeline.hpp>
#include <floor/graphics/graphics_pass.hpp>
#include <floor/graphics/graphics_renderer.hpp>

const compute_device* compute_context::get_device(const compute_device::TYPE type) const {
	switch(type) {
		case compute_device::TYPE::ANY:
			// just return the first valid device if one exists
			return (!devices.empty() ? devices[0].get() : nullptr);
		case compute_device::TYPE::FASTEST:
			if(fastest_device) return fastest_device;
			break;
		case compute_device::TYPE::FASTEST_GPU:
			if(fastest_gpu_device) return fastest_gpu_device;
			break;
		case compute_device::TYPE::FASTEST_CPU:
			if(fastest_cpu_device) return fastest_cpu_device;
			break;
		case compute_device::TYPE::FASTEST_FLAG:
		case compute_device::TYPE::NONE:
		case compute_device::TYPE::ALL_CPU:
		case compute_device::TYPE::ALL_GPU:
		case compute_device::TYPE::ALL_DEVICES:
			log_warn("shouldn't use type %X to get a device!", type);
			return (!devices.empty() ? devices[0].get() : nullptr);
		default: break;
	}
	
	typedef underlying_type_t<compute_device::TYPE> enum_type;
	enum_type counter = 0;
	if(type >= compute_device::TYPE::GPU0 && type <= compute_device::TYPE::GPU255) {
		const enum_type num = (enum_type)type - (enum_type)compute_device::TYPE::GPU0;
		for(const auto& dev : devices) {
			if(((enum_type)dev->type & (enum_type)compute_device::TYPE::GPU) != 0) {
				if(num == counter) return dev.get();
				++counter;
			}
		}
	}
	else if(type >= compute_device::TYPE::CPU0 && type <= compute_device::TYPE::CPU255) {
		const enum_type num = (enum_type)type - (enum_type)compute_device::TYPE::CPU0;
		for(const auto& dev : devices) {
			if(((enum_type)dev->type & (enum_type)compute_device::TYPE::CPU) != 0) {
				if(num == counter) return dev.get();
				++counter;
			}
		}
	}
	// else: didn't find any or type is a weird mixture
	log_error("couldn't find a device matching the specified type %X, returning the first device instead!", type);
	return (!devices.empty() ? devices[0].get() : nullptr);
}

vector<const compute_device*> compute_context::get_devices() const {
	vector<const compute_device*> ret;
	for (const auto& dev : devices) {
		ret.emplace_back(dev.get());
	}
	return ret;
}

const compute_device* compute_context::get_corresponding_device(const compute_device& external_dev) const {
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

shared_ptr<compute_buffer> compute_context::wrap_buffer(const compute_queue&, vulkan_buffer&, const COMPUTE_MEMORY_FLAG) const {
	log_error("Vulkan buffer sharing is not supported by this backend");
	return {};
}

shared_ptr<compute_image> compute_context::wrap_image(const compute_queue&, vulkan_image&, const COMPUTE_MEMORY_FLAG) const {
	log_error("Vulkan image sharing is not supported by this backend");
	return {};
}

shared_ptr<compute_buffer> compute_context::wrap_buffer(const compute_queue&, metal_buffer&, const COMPUTE_MEMORY_FLAG) const {
	log_error("Metal buffer sharing is not supported by this backend");
	return {};
}

shared_ptr<compute_image> compute_context::wrap_image(const compute_queue&, metal_image&, const COMPUTE_MEMORY_FLAG) const {
	log_error("Metal image sharing is not supported by this backend");
	return {};
}

unique_ptr<graphics_pipeline> compute_context::create_graphics_pipeline(const render_pipeline_description&) const {
	log_error("graphics not supported by this backend");
	return {};
}

unique_ptr<graphics_pass> compute_context::create_graphics_pass(const render_pass_description&) const {
	log_error("graphics not supported by this backend");
	return {};
}

unique_ptr<graphics_renderer> compute_context::create_graphics_renderer(const compute_queue&, const graphics_pass&, const graphics_pipeline&,
																		const bool) const {
	log_error("graphics not supported by this backend");
	return {};
}

COMPUTE_IMAGE_TYPE compute_context::get_renderer_image_type() const {
	return COMPUTE_IMAGE_TYPE::NONE;
}

uint4 compute_context::get_renderer_image_dim() const {
	return {};
}

vr_context* compute_context::get_renderer_vr_context() const {
	return nullptr;
}

void compute_context::set_hdr_metadata(const hdr_metadata_t& hdr_metadata_) {
	hdr_metadata = hdr_metadata_;
}

const hdr_metadata_t& compute_context::get_hdr_metadata() const {
	return hdr_metadata;
}

const float2& compute_context::get_hdr_luminance_range() const {
	return hdr_metadata.luminance;
}

float compute_context::get_hdr_range_max() const {
	return 1.0f;
}

float compute_context::get_hdr_display_max_nits() const {
	return 80.0f;
}
