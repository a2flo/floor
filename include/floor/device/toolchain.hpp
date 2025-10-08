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

#include <floor/core/essentials.hpp>
#include <floor/device/device.hpp>
#include <memory>
#include <optional>

namespace fl::toolchain {
	//! compilation target platform
	enum class TARGET {
		//! OpenCL SPIR 1.2
		SPIR,
		//! Nvidia CUDA PTX 8.0+
		PTX,
		//! Metal Apple-IR 3.0+
		AIR,
		//! Vulkan SPIR-V 1.6+
		SPIRV_VULKAN,
		//! OpenCL SPIR-V 1.0+
		SPIRV_OPENCL,
		//! Host-Compute CPU
		HOST_COMPUTE_CPU,
	};

	//! known function types
	enum class FUNCTION_TYPE : uint32_t {
		NONE							= (0u),
		KERNEL							= (1u),
		VERTEX							= (2u),
		FRAGMENT						= (3u),
		//! NOTE: for internal use only
		TESSELLATION_CONTROL			= (4u),
		//! aka "post-tessellation vertex shader"
		TESSELLATION_EVALUATION			= (5u),
		
		//! argument buffer structs are treated the same way as actual functions
		ARGUMENT_BUFFER_STRUCT			= (100u),
	};

	//! flags applying to the whole function
	enum class FUNCTION_FLAGS : uint32_t {
		NONE							= (0u),
		//! function makes use of soft-printf
		USES_SOFT_PRINTF				= (1u << 0u),
		//! kernel dimensionality
		KERNEL_1D						= (1u << 1u),
		KERNEL_2D						= (1u << 2u),
		KERNEL_3D						= (1u << 3u),
		//! Vulkan-only: low inline uniform block count
		VULKAN_LOW_IUB					= (1u << 4u),
		//! Vulkan-only: low descriptor set count
		VULKAN_LOW_DS					= (1u << 5u),
	};
	floor_global_enum_ext(FUNCTION_FLAGS)

	//! address space
	enum class ARG_ADDRESS_SPACE : uint32_t {
		UNKNOWN							= (0u),
		GLOBAL							= (1u),
		LOCAL							= (2u),
		CONSTANT						= (3u),
		IMAGE							= (4u),
	};

	//! image type
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

	//! r/w memory/image access flags
	enum class ARG_ACCESS : uint32_t {
		UNSPECIFIED						= (0u),
		READ							= (1u << 0u),
		WRITE							= (1u << 1u),
		READ_WRITE						= (READ | WRITE),
	};

	//! special argument flags (backend or function type specific)
	enum class ARG_FLAG : uint32_t {
		NONE							= (0u),
		//! an array of some sort (buffers, images, plain data)
		ARRAY							= (1u << 0u),
		//! array of images
		IMAGE_ARRAY						= (1u << 1u),
		//! Vulkan/Metal only: array of buffers
		//! NOTE: Vulkan: always used, Metal: only used for buffer arrays in argument buffers
		BUFFER_ARRAY					= (1u << 2u),
		//! argument/indirect buffer
		ARGUMENT_BUFFER					= (1u << 3u),
		//! graphics-only: shader stage input
		STAGE_INPUT						= (1u << 4u),
		//! Vulkan-only: constant parameter fast path
		PUSH_CONSTANT					= (1u << 5u),
		//! Vulkan-only: param is a storage buffer (not uniform)
		SSBO							= (1u << 6u),
		//! Vulkan-only: inline uniform block
		IUB								= (1u << 7u),
	};
	floor_global_enum_ext(ARG_FLAG)

	//! need forward decl for function_info
	struct arg_info;

	//! this contains all necessary information of a function (types, args, arg types, sizes, ...)
	struct function_info {
		std::string name;
		
		//! required local size/dim needed for execution
		//! NOTE: if any component is 0, the local size is considered unspecified
		uint3 required_local_size { 0u };
		constexpr bool has_valid_required_local_size() const {
			return (required_local_size != 0u).all();
		}
		
		//! required SIMD-width (if non-zero)
		//! NOTE: if this is 0, the SIMD-width is considered unspecified
		uint32_t required_simd_width { 0u };
		constexpr bool has_valid_required_simd_width() const {
			return (required_simd_width != 0u);
		}
		
		FUNCTION_TYPE type { FUNCTION_TYPE::NONE };
		
		FUNCTION_FLAGS flags { FUNCTION_FLAGS::NONE };
		
		std::vector<arg_info> args;
		
		//! true if this function comes from a precompiled FUBAR binary,
		//! false if it was compiled at run-time
		bool is_fubar { false };
		//! if "is_fubar" is true and this is a Vulkan function/binary, this contains the max allowed mip level count
		uint32_t max_mip_levels { 0u };
		
		//! returns the kernel dimensionality
		uint32_t get_kernel_dim() const {
			if (has_flag<toolchain::FUNCTION_FLAGS::KERNEL_3D>(flags)) {
				return 3u;
			} else if (has_flag<toolchain::FUNCTION_FLAGS::KERNEL_2D>(flags)) {
				return 2u;
			}
			// either KERNEL_1D or not a kernel -> just return 1
			return 1u;
		}
	};

	//! argument information
	struct arg_info {
		//! sizeof(arg_type) if applicable
		uint64_t size { 0u };
		
		//! array extent if "is_array()" is true
		//! NOTE: "size" includes the size of all array elements, not just a single one
		//!       -> can divide by "array_extent" to get the individual element size
		uint64_t array_extent { 0u };
		
		//! NOTE: this will only be correct for OpenCL/Metal/Vulkan, CUDA uses a different approach,
		//!       although some arguments might be marked with an address space nonetheless
		ARG_ADDRESS_SPACE address_space { ARG_ADDRESS_SPACE::GLOBAL };
		
		//! memory access of the argument (UNSPECIFIED if unknown or not applicable)
		ARG_ACCESS access { ARG_ACCESS::UNSPECIFIED };
		
		//! the specific image type if this argument is an image (otherwise NONE)
		ARG_IMAGE_TYPE image_type { ARG_IMAGE_TYPE::NONE };
		
		//! special argument flags
		ARG_FLAG flags { ARG_FLAG::NONE };
		
		//! true if this argument is some sort of array, i.e. array_extent contains a valid array extent
		bool is_array() const {
			return (has_flag<ARG_FLAG::ARRAY>(flags) ||
					has_flag<ARG_FLAG::BUFFER_ARRAY>(flags) ||
					has_flag<ARG_FLAG::IMAGE_ARRAY>(flags));
		}
		
		//! if this is an argument buffer (special_type == ARGUMENT_BUFFER) then this contains the argument buffer struct info
		std::optional<function_info> argument_buffer_info {};
	};
	
	//! internal: packed version of the image support flags
	enum class IMAGE_CAPABILITY : uint32_t {
		NONE					= (0u),
		BASIC					= (1u << 0u),
		
		DEPTH_READ				= (1u << 1u),
		DEPTH_WRITE				= (1u << 2u),
		MSAA_READ				= (1u << 3u),
		MSAA_WRITE				= (1u << 4u),
		MSAA_ARRAY_READ			= (1u << 5u),
		MSAA_ARRAY_WRITE		= (1u << 6u),
		CUBE_READ				= (1u << 7u),
		CUBE_WRITE				= (1u << 8u),
		CUBE_ARRAY_READ			= (1u << 9u),
		CUBE_ARRAY_WRITE		= (1u << 10u),
		MIPMAP_READ				= (1u << 11u),
		MIPMAP_WRITE			= (1u << 12u),
		OFFSET_READ				= (1u << 13u),
		OFFSET_WRITE			= (1u << 14u),
		
		DEPTH_COMPARE			= (1u << 20u),
		GATHER					= (1u << 21u),
		READ_WRITE				= (1u << 22u),
	};
	floor_global_enum_ext(IMAGE_CAPABILITY)
	
	//! compilation options that will either be passed through to the compiler or enable/disable internal behavior
	struct compile_options {
		//! the compilation target platform
		//! NOTE: unless calling toolchain directly, this does not have to be set (i.e. this is handled by each backend)
		TARGET target { TARGET::SPIR };
		
		//! options that are directly passed through to the compiler
		std::string cli { "" };
		
		//! if true, enables the default set of warning flags
		bool enable_warnings { false };
		
		//! if true, overrides the config compute.log_commands option and silences other debug output
		bool silence_debug_output { false };
		
		//! ignore changing compile settings based on querying these at runtime
		//! e.g. OS, CPU features, ...
		bool ignore_runtime_info { false };
		
		//! when building a FUBAR archive: compress all binary data in the archive?
		bool compress_binaries { false };
		
		//! if true, enables C assert() functionality,
		//! i.e. if an assertion doesn't hold true, the function is exited and an error is printed
		//! NOTE: printing requires soft-printf to be enabled on Metal/Vulkan/Host-Compute
		bool enable_assert { false };
		
		//! debug options
		struct {
			//! if true, enables the emission of target dependent debug info,
			//! i.e. for most targets this will enable -gline-tables-only,
			//! for others (e.g. Metal) this will enable other debug options as well
			bool emit_debug_info { false };
			
			//! if true, preprocesses the input (-E) and condenses it into a single .ii file
			//! before compiling it into the target specific format/binary
			//! NOTE: this is useful when debugging capabilities are limited to a single file
			//! NOTE: only available for Metal targets
			bool preprocess_condense { false };
			
			//! if true and "preprocess_condense" is enabled, this will not remove
			//! all comments from the preprocessed .ii file
			//! NOTE: only available for Metal targets
			bool preprocess_preserve_comments { false };
		} debug;
		
		//! CUDA specific options
		struct {
			//! sets the PTX version that should be used (8.0 by default)
			uint32_t ptx_version { 80 };
			
			//! sets the maximum amount of registers that may be used
			//! if 0, the global config setting is used
			uint32_t max_registers { 0u };
			
			//! use short/32-bit pointers for accessing non-global memory
			bool short_ptr { true };
		} cuda;
		
		//! Metal specific options
		struct {
			//! if true, enable soft-printf support
			//! if unset, use the global floor option
			std::optional<bool> soft_printf;
			
			//! restricts rather than enables various scalar->vector transformations
			//! NOTE: enabling this puts the vectorization on the same level as Vulkan
			bool restrictive_vectorization { false };
		} metal;
		
		//! Vulkan specific options
		struct {
			//! if true, enable soft-printf support
			//! if unset, use the global floor option
			std::optional<bool> soft_printf;
			
			//! performs an LLVM CFG structurization pass prior to the actual structurization
			//! NOTE: use this with caution, i.e. only try to use this if the default structurization failed,
			//!       since this pass may itself transform the CFG into an unstructured CFG that can not be recovered
			bool pre_structurization_pass { false };
			
			//! run spirv-opt after toolchain compilation
			//! NOTE: this is experimental as spirv-opt may introduce errors
			bool run_opt { false };
			
			//! if specified, this must contain valid options that will be passed to spirv-opt
			//! NOTE: this will override the default options!
			std::optional<std::string> opt_overrides;
			
			//! if true, enables workarounds for certain pointer uses in SPIR-V
			//! NOTE: it is generally not recommended to enable this as this will cause performance and potential correctness issues
			bool pointer_workarounds { false };
		} vulkan;
		
		//! optional pre-compiled header that should be used for compilation
		//! NOTE: the caller *must* ensure that the pch is compatible to the current compile options and the target device
		std::optional<std::string> pch;
	};
	
	//! contains all information about a compiled compute/graphics program
	struct program_data {
		//! true if compilation was successful and this contains valid program data, false otherwise
		bool valid { false };
		
		//! this either contains the compiled binary data (for PTX, SPIR),
		//! or the filename to the compiled binary (SPIR-V, AIR)
		std::string data_or_filename;
		
		//! contains the function-specific information for all functions in the program
		std::vector<function_info> function_info;
		
		//! the options that were used to compile this program
		compile_options options;
	};
	
	//! compiles a program from a source code string
	program_data compile_program(const device& dev,
								 const std::string& code,
								 const compile_options options);
	//! compiles a program from a source file
	program_data compile_program_file(const device& dev,
									  const std::string& filename,
									  const compile_options options);
	
	//! compiles a pre-compiled header for the specified "device" using the specified "options",
	//! the output pch will be written to "pch_output_file_name"
	program_data compile_precompiled_header(const std::string& pch_output_file_name,
											const device& dev,
											const compile_options options);
	
	//! creates the internal floor function info representation from the specified floor function info,
	//! returns true on success
	bool create_floor_function_info(const std::string& ffi_file_name,
									std::vector<function_info>& functions,
									const uint32_t toolchain_version);

} // fl::toolchain
