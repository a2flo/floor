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
	//! compilation target platform
	enum class TARGET {
		//! OpenCL SPIR 1.2
		SPIR,
		//! Nvidia CUDA PTX 4.3+
		PTX,
		//! Metal Apple-IR 1.1
		AIR,
		//! Apple OpenCL 3.2
		APPLECL,
		//! Vulkan SPIR-V 1.0
		SPIRV_VULKAN,
		//! OpenCL SPIR-V 1.0
		SPIRV_OPENCL,
	};
	
	//! this contains all necessary information of a function (types, args, arg types, sizes, ...)
	struct function_info {
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
			//! graphics-only: vertex/fragment shader stage input
			STAGE_INPUT						= (1u),
			//! vulkan-only: constant parameter fast path
			PUSH_CONSTANT					= (2u),
		};
		
		struct arg_info {
			uint32_t size;
			
			//! NOTE: this will only be correct for OpenCL and Metal, CUDA uses a different approach,
			//! although some arguments might be marked with an address space nonetheless.
			ARG_ADDRESS_SPACE address_space { ARG_ADDRESS_SPACE::GLOBAL };
			
			ARG_IMAGE_TYPE image_type { ARG_IMAGE_TYPE::NONE };
			
			ARG_IMAGE_ACCESS image_access { ARG_IMAGE_ACCESS::NONE };
			
			SPECIAL_TYPE special_type { SPECIAL_TYPE::NONE };
		};
		vector<arg_info> args;
	};
	
	//! internal: encoded floor metadata layout
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
	
	//! internal: packed version of the image support flags
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
	
	//! compilation options that will either be passed through to the compiler or enable/disable internal behavior
	struct compile_options {
		//! the compilation target platform
		//! NOTE: unless calling llvm_compute directly, this does not have to be set (i.e. this is handled by each backend)
		TARGET target { TARGET::SPIR };
		
		//! options that are directly passed through to the compiler
		string cli { "" };
		
		//! if true, sets the default set of warning flags
		bool enable_warnings { false };
		
		//! cuda: sets the maximum amount of registers that may be used
		//!       if 0, the global config setting is used
		uint32_t cuda_max_registers { 0u };
	};
	
	//! contains all information about a compiled compute/graphics program
	struct program_data {
		//! true if compilation was successful and this contains valid program data, false otherwise
		bool valid { false };
		
		//! this either contains the compiled binary data (for PTX, SPIR, APPLECL),
		//! or the filename to the compiled binary (SPIR-V, AIR)
		string data_or_filename;
		
		//! contains the function-specific information for all functions in the program
		vector<function_info> functions;
		
		//! the options that were used to compile this program
		compile_options options;
	};
	
	//! compiles a program from a source code string
	static program_data compile_program(shared_ptr<compute_device> device,
										const string& code,
										const compile_options options);
	//! compiles a program from a source file
	static program_data compile_program_file(shared_ptr<compute_device> device,
											 const string& filename,
											 const compile_options options);
	
	//! compiles a program from the specified input file/handle and prefixes the compiler call with "cmd_prefix"
	static program_data compile_input(const string& input,
									  const string& cmd_prefix,
									  shared_ptr<compute_device> device,
									  const compile_options options);
	
	//! creates the internal floor function info representation from the specified floor function info,
	//! returns true on success
	static bool create_floor_function_info(const string& ffi_file_name,
										   vector<llvm_compute::function_info>& functions,
										   const uint32_t toolchain_version);
	
	//! loads a spir-v binary from the file specified by file_name,
	//! returning the read code + setting code_size to the amount of bytes
	//! TODO: add proper helper class
	static unique_ptr<uint32_t[]> load_spirv_binary(const string& file_name, size_t& code_size);
	
protected:
	// static class
	llvm_compute(const llvm_compute&) = delete;
	~llvm_compute() = delete;
	llvm_compute& operator=(const llvm_compute&) = delete;
	
};

#endif
