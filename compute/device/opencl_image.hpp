/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_OPENCL_IMAGE_HPP__
#define __FLOOR_COMPUTE_DEVICE_OPENCL_IMAGE_HPP__

#if defined(FLOOR_COMPUTE_OPENCL)

namespace opencl_image {
	//////////////////////////////////////////
	// opencl sampler types/modes
	namespace sampler {
		enum ADDRESS_MODE : uint32_t {
			NONE			= 0,
			CLAMP_TO_ZERO	= 4,
			CLAMP_TO_EDGE	= 2,
			REPEAT			= 6,
			MIRRORED_REPEAT	= 8
		};
		enum FILTER_MODE : uint32_t {
			NEAREST			= 0x10,
			LINEAR			= 0x20
		};
		enum COORD_MODE : uint32_t {
			PIXEL		= 0,
			NORMALIZED	= 1
		};
	};
	
}

#endif

#endif
