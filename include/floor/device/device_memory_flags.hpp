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
#include <floor/core/enum_helpers.hpp>
#include <cstdint>

namespace fl {

//! memory flags
enum class MEMORY_FLAG : uint32_t {
	//! invalid/uninitialized flag
	NONE						= (0u),
	
	//! read only memory (function point of view)
	READ						= (1u << 0u),
	//! write only memory (function point of view)
	WRITE						= (1u << 1u),
	//! read and write memory (function point of view)
	READ_WRITE					= (READ | WRITE),
	
	//! read only memory (host point of view)
	HOST_READ					= (1u << 2u),
	//! write only memory (host point of view)
	HOST_WRITE					= (1u << 3u),
	//! read and write memory (host point of view)
	HOST_READ_WRITE				= (HOST_READ | HOST_WRITE),
	//! if neither HOST_READ or HOST_WRITE is set, the host will not have access to the memory
	//! -> can use this mask to AND with flags
	__HOST_NO_ACCESS_MASK		= ~(HOST_READ_WRITE),
	
	//! the memory will use/store the specified host pointer,
	//! but won't initialize the device memory with that data
	NO_INITIAL_COPY				= (1u << 4u),
	
	//! optimizes the allocation for host read back (device -> host copy)
	//! NOTE: likely performs a host-side allocation that is sub-optimal for device-side operations
	HOST_READ_BACK_OPTIMIZE		= (1u << 5u),
	
	//! when beneficial, creates a temporary staging buffer during device -> host reads/copies that is optimized for host read back,
	//! copying the device memory to the staging buffer first before copying from it to the host
	HOST_READ_STAGING			= (1u << 6u),
	
	//! memory is allocated in host memory, i.e. the specified host pointer
	//! will be used for all memory operations
	USE_HOST_MEMORY				= (1u << 7u),
	
	//! internal flag that is implicitly set when creating an image with IMAGE_TYPE::FLAG_RENDER_TARGET,
	//! if this is set, this allows neither READ or WRITE to be set, making the render target not directly readable/writable from a function
	//! NOTE: usable for images only
	//! NOTE: Vulkan/Metal only
	RENDER_TARGET				= (1u << 8u),
	
	//! automatically create mip-levels (either happens in the backend or libfloor)
	//! NOTE: if not set, it is expected that the host data pointer contains all necessary mip-levels
	//! NOTE: of course, this flag only makes sense for device_images
	GENERATE_MIP_MAPS			= (1u << 9u),
	
	//! creates the memory with Vulkan sharing enabled
	//! NOTE: the Vulkan object can be retrieved via get_vulkan_buffer()/get_vulkan_image()
	//! NOTE: VULKAN_SHARING and USE_HOST_MEMORY are mutually exclusive (for obvious reasons)
	VULKAN_SHARING				= (1u << 10u),
	
	//! creates the memory with Metal sharing enabled
	//! NOTE: the Metal object can be retrieved via get_metal_buffer()/get_metal_image()
	//! NOTE: METAL_SHARING and USE_HOST_MEMORY are mutually exclusive (for obvious reasons)
	METAL_SHARING				= (1u << 11u),
	
	//! automatically synchronizes the contents of the memory of the memory object with the shared Vulkan memory,
	//! i.e. when using the memory in a Vulkan function execution with the memory currently being acquired for compute use,
	//! automatically copy the current contents of the memory object to the shared Vulkan memory object
	//! NOTE: only functional for Host-Compute <-> Vulkan interop, this is not needed for CUDA <-> Vulkan interop (backed by the same memory)
	//! NOTE: this is only intended for reading data on the Vulkan side (no write-back will happen)
	//! NOTE: prefer using SHARING_SYNC + specific r/w flags instead
	VULKAN_SHARING_SYNC_SHARED	= (1u << 12u),
	
	//! automatically synchronizes the contents of the memory of the memory object with the shared Metal memory,
	//! i.e. when using the memory in a Metal function execution with the memory currently being acquired for compute use,
	//! automatically copy the current contents of the memory object to the shared Metal memory object
	//! NOTE: this is only intended for reading data on the Metal side (no write-back will happen)
	//! NOTE: prefer using SHARING_SYNC + specific r/w flags instead
	METAL_SHARING_SYNC_SHARED	= (1u << 13u),
	
	//! Vulkan-only: creates images/buffers with memory aliasing support
	//! NOTE: for array images, this will automatically create aliased single-plane images of the whole image array
	VULKAN_ALIASING				= (1u << 14u),
	
	//! Vulkan-only: allocate memory in device-local / host-coherent memory
	//! NOTE: must have HOST_READ and/or HOST_WRITE set
	VULKAN_HOST_COHERENT		= (1u << 15u),
	
	//! Metal-only: disables any automatic resource tracking on the allocated Metal object
	//! NOTE: may be used for other backends as well in the future
	NO_RESOURCE_TRACKING		= (1u << 16u),
	
	//! Vulkan-only: allocates a buffer with support for being used as a descriptor buffer
	VULKAN_DESCRIPTOR_BUFFER	= (1u << 17u),
	
	//! with VULKAN_SHARING/METAL_SHARING: automatically synchronizes (writes back) the contents between the shared Metal/Vulkan memory and
	//! the memory object when the memory is used in functions, under consideration of render and compute backend specfic read/write flags
	//! NOTE: only functional for Host-Compute <-> Vulkan/Metal interop, not needed when the memory backing is physically the same
	//! NOTE: needs to set appropriate SHARING_RENDER_* and SHARING_COMPUTE_* flags, otherwise it is assumed everything is r/w
	SHARING_SYNC				= (1u << 18u),
	
	//! with SHARING_SYNC: render backend only reads memory from the compute backend
	SHARING_RENDER_READ			= (1u << 19u),
	//! with SHARING_SYNC: render backend only writes memory for the compute backend
	SHARING_RENDER_WRITE		= (1u << 20u),
	//! with SHARING_SYNC: render backend reads and writes memory from/for the compute backend
	//! NOTE: this is the default
	SHARING_RENDER_READ_WRITE	= (SHARING_RENDER_READ | SHARING_RENDER_WRITE),
	
	//! with SHARING_SYNC: compute backend only reads memory from the render backend
	SHARING_COMPUTE_READ		= (1u << 21u),
	//! with SHARING_SYNC: compute backend only writes memory for the render backend
	SHARING_COMPUTE_WRITE		= (1u << 22u),
	//! with SHARING_SYNC: compute backend reads and writes memory from/for the render backend
	//! NOTE: this is the default
	SHARING_COMPUTE_READ_WRITE	= (SHARING_COMPUTE_READ | SHARING_COMPUTE_WRITE),
	
	//! Metal/Vulkan-only: request a heap allocation when heap allocations are disabled by default in the context
	//! NOTE: to be used in conjunction with DEVICE_CONTEXT_FLAGS::EXPLICIT_HEAP
	//! NOTE: mutually exclusive with NO_HEAP_ALLOCATION
	HEAP_ALLOCATION				= (1u << 23u),
	
	//! Metal/Vulkan-only: explicitly request a dedicated allocation when heap allocations are enabled by default in the context
	//! NOTE: mutually exclusive with HEAP_ALLOCATION
	NO_HEAP_ALLOCATION			= (1u << 24u),
	
	//! Vulkan-only: in situations where not enough device-local host-coherent/host-accessible memory is available,
	//! allow an allocation to be made in host memory (not device-local)
	VULKAN_MAY_USE_HOST_MEMORY	= (1u << 25u),
	
};
floor_global_enum_ext(MEMORY_FLAG)

//! memory mapping flags
enum class MEMORY_MAP_FLAG : uint32_t {
	NONE				= (0u),
	READ				= (1u << 0u),
	WRITE				= (1u << 1u),
	WRITE_INVALIDATE	= (1u << 2u),
	READ_WRITE			= (READ | WRITE),
	BLOCK				= (1u << 3u),
};
floor_global_enum_ext(MEMORY_MAP_FLAG)

} // namespace fl
