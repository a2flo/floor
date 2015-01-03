/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#include <floor/compute/compute_base.hpp>

compute_base::~compute_base() {}

shared_ptr<compute_device> compute_base::get_device(const compute_device::TYPE type) const {
	switch(type) {
		case compute_device::TYPE::ANY:
			// just return the first valid device if one exists
			return (!devices.empty() ? devices[0] : nullptr);
		case compute_device::TYPE::FASTEST:
			return fastest_device; // note, no check here, b/c the result would be the same
		case compute_device::TYPE::FASTEST_GPU:
		case compute_device::TYPE::GPU: // shouldn't use this like that
			if(fastest_gpu_device) return fastest_gpu_device;
			break;
		case compute_device::TYPE::FASTEST_CPU:
		case compute_device::TYPE::CPU: // shouldn't use this like that
			if(fastest_cpu_device) return fastest_cpu_device;
			break;
		case compute_device::TYPE::FASTEST_FLAG:
		case compute_device::TYPE::NONE:
		case compute_device::TYPE::ALL_CPU:
		case compute_device::TYPE::ALL_GPU:
		case compute_device::TYPE::ALL_DEVICES:
			log_warn("shouldn't use type %X to get a device!", type);
			return (!devices.empty() ? devices[0] : nullptr);
		default: break;
	}
	
	typedef underlying_type_t<compute_device::TYPE> enum_type;
	enum_type counter = 0;
	if(type >= compute_device::TYPE::GPU0 && type <= compute_device::TYPE::GPU255) {
		const enum_type num = (enum_type)type - (enum_type)compute_device::TYPE::GPU0;
		for(const auto& dev : devices) {
			if(((enum_type)dev->type & (enum_type)compute_device::TYPE::GPU) != 0) {
				if(num == counter) return dev;
				++counter;
			}
		}
	}
	else if(type >= compute_device::TYPE::CPU0 && type <= compute_device::TYPE::CPU255) {
		const enum_type num = (enum_type)type - (enum_type)compute_device::TYPE::CPU0;
		for(const auto& dev : devices) {
			if(((enum_type)dev->type & (enum_type)compute_device::TYPE::CPU) != 0) {
				if(num == counter) return dev;
				++counter;
			}
		}
	}
	// else: didn't find any or type is a weird mixture
	log_error("couldn't find a device matching the specified type %X, returning the first device instead!", type);
	return (!devices.empty() ? devices[0] : nullptr);
}
