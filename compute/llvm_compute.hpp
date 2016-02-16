/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#ifndef __FLOOR_LLVM_COMPUTE_HPP__
#define __FLOOR_LLVM_COMPUTE_HPP__

#include <floor/core/essentials.hpp>
#include <floor/compute/compute_device.hpp>
#include <memory>

class llvm_compute {
public:
	enum class TARGET {
		//! OpenCL SPIR 1.2
		SPIR,
		//! Nvidia CUDA PTX
		PTX,
		//! Metal Apple-IR
		AIR,
		//! Apple OpenCL (3.2)
		APPLECL,
		//! Vulkan/OpenCL SPIRV
		SPIRV,
	};
	
	//
	struct kernel_info {
		string name;
		
		enum class FUNCTION_TYPE : uint32_t {
			NONE							= (0u),
			KERNEL							= (1u),
			VERTEX							= (2u),
			FRAGMENT						= (3u),
		};
		FUNCTION_TYPE type { FUNCTION_TYPE::NONE };
		
		enum class ARG_ADDRESS_SPACE : uint32_t {
			UNKNOWN							= (0u),
			GLOBAL							= (1u),
			LOCAL							= (2u),
			CONSTANT						= (3u),
			IMAGE							= (4u),
		};
		
		enum class ARG_IMAGE_TYPE : uint32_t {
			NONE							= (0u),
			IMAGE_1D						= (1u),
			IMAGE_1D_ARRAY					= (2u),
			IMAGE_1D_BUFFER					= (3u),
			IMAGE_2D						= (4u),
			IMAGE_2D_ARRAY					= (5u),
			IMAGE_2D_DEPTH					= (6u),
			IMAGE_2D_ARRAY_DEPTH			= (7u),
			IMAGE_2D_MSAA					= (8u),
			IMAGE_2D_ARRAY_MSAA				= (9u),
			IMAGE_2D_MSAA_DEPTH				= (10u),
			IMAGE_2D_ARRAY_MSAA_DEPTH		= (11u),
			IMAGE_3D						= (12u),
			IMAGE_CUBE						= (13u),
			IMAGE_CUBE_ARRAY				= (14u),
			IMAGE_CUBE_DEPTH				= (15u),
			IMAGE_CUBE_ARRAY_DEPTH			= (16u),
		};
		
		enum class ARG_IMAGE_ACCESS : uint32_t {
			NONE							= (0u),
			READ							= (1u),
			WRITE							= (2u),
			READ_WRITE						= (READ | WRITE),
		};
		
		enum class SPECIAL_TYPE : uint32_t {
			NONE							= (0u),
			STAGE_INPUT						= (1u),
		};
		
		struct kernel_arg_info {
			uint32_t size;
			
			//! NOTE: this will only be correct for OpenCL and Metal, CUDA uses a different approach,
			//! although some arguments might be marked with an address space nonetheless.
			ARG_ADDRESS_SPACE address_space { ARG_ADDRESS_SPACE::GLOBAL };
			
			ARG_IMAGE_TYPE image_type { ARG_IMAGE_TYPE::NONE };
			
			ARG_IMAGE_ACCESS image_access { ARG_IMAGE_ACCESS::NONE };
			
			SPECIAL_TYPE special_type { SPECIAL_TYPE::NONE };
		};
		vector<kernel_arg_info> args;
	};
	
	// for internal use
	enum class FLOOR_METADATA : uint64_t {
		ARG_SIZE_MASK			= (0x00000000FFFFFFFFull),
		ARG_SIZE_SHIFT			= (0ull),
		ADDRESS_SPACE_MASK		= (0x0000000700000000ull),
		ADDRESS_SPACE_SHIFT		= (32ull),
		IMAGE_TYPE_MASK			= (0x0000FF0000000000ull),
		IMAGE_TYPE_SHIFT		= (40ull),
		IMAGE_ACCESS_MASK		= (0x0003000000000000ull),
		IMAGE_ACCESS_SHIFT		= (48ull),
		SPECIAL_TYPE_MASK		= (0xFF00000000000000ull),
		SPECIAL_TYPE_SHIFT		= (56ull),
	};
	
	// packed version of the image support flags
	enum class IMAGE_CAPABILITY : uint32_t {
		NONE					= (0u),
		BASIC					= (1u << 0u),
		DEPTH_READ				= (1u << 1u),
		DEPTH_WRITE				= (1u << 2u),
		MSAA_READ				= (1u << 3u),
		MSAA_WRITE				= (1u << 4u),
		CUBE_READ				= (1u << 5u),
		CUBE_WRITE				= (1u << 6u),
		MIPMAP_READ				= (1u << 7u),
		MIPMAP_WRITE			= (1u << 8u),
		OFFSET_READ				= (1u << 9u),
		OFFSET_WRITE			= (1u << 10u),
	};
	floor_enum_ext(IMAGE_CAPABILITY)
	
	//!
	static pair<string, vector<kernel_info>> compile_program(shared_ptr<compute_device> device,
															 const string& code,
															 const string additional_options = "",
															 const TARGET target = TARGET::SPIR);
	//!
	static pair<string, vector<kernel_info>> compile_program_file(shared_ptr<compute_device> device,
																  const string& filename,
																  const string additional_options = "",
																  const TARGET target = TARGET::SPIR);
	//!
	static pair<string, vector<kernel_info>> compile_input(const string& input,
														   const string& cmd_prefix,
														   shared_ptr<compute_device> device,
														   const string additional_options = "",
														   const TARGET target = TARGET::SPIR);
	
	//! extracts the floor metadata (kernel_info) from the specified llvm ir, returns true on success
	static bool get_floor_metadata(const string& llvm_ir, vector<llvm_compute::kernel_info>& kernels);
	
protected:
	// static class
	llvm_compute(const llvm_compute&) = delete;
	~llvm_compute() = delete;
	llvm_compute& operator=(const llvm_compute&) = delete;
	
};

#endif
