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

#include <floor/compute/metal/metal_kernel.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/compute/compute_context.hpp>
#include <floor/compute/metal/metal_compute.hpp>
#include <floor/compute/metal/metal_queue.hpp>
#include <floor/compute/metal/metal_buffer.hpp>
#include <floor/compute/metal/metal_image.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/metal/metal_args.hpp>
#include <floor/compute/metal/metal_argument_buffer.hpp>
#include <floor/compute/metal/metal_fence.hpp>
#include <floor/compute/soft_printf.hpp>

struct metal_encoder {
	id <MTLCommandBuffer> cmd_buffer { nil };
	id <MTLComputeCommandEncoder> encoder { nil };
};

static unique_ptr<metal_encoder> create_encoder(const compute_queue& cqueue,
												const metal_kernel::metal_kernel_entry& entry,
												const char* debug_label) {
	id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
	id <MTLComputeCommandEncoder> encoder;
	if (@available(macOS 10.14, iOS 12.0, *)) {
		encoder = [cmd_buffer computeCommandEncoderWithDispatchType:MTLDispatchTypeConcurrent];
	} else {
		encoder = [cmd_buffer computeCommandEncoder];
	}
	auto ret = make_unique<metal_encoder>(metal_encoder { cmd_buffer, encoder });
	[ret->encoder setComputePipelineState:(__bridge id <MTLComputePipelineState>)entry.kernel_state];
	if (debug_label) {
		[ret->encoder setLabel:[NSString stringWithUTF8String:debug_label]];
	}
	return ret;
}

metal_kernel::metal_kernel(kernel_map_type&& kernels_) : kernels(move(kernels_)) {
}

pair<uint3, uint3> metal_kernel::compute_grid_and_block_dim(const kernel_entry& entry,
															const uint32_t& dim,
															const uint3& global_work_size,
															const uint3& local_work_size) const {
	// check work size (NOTE: will set elements to at least 1)
	const auto block_dim = check_local_work_size(entry, local_work_size);
	const uint3 grid_dim_overflow {
		dim >= 1 && global_work_size.x > 0 ? std::min(uint32_t(global_work_size.x % block_dim.x), 1u) : 0u,
		dim >= 2 && global_work_size.y > 0 ? std::min(uint32_t(global_work_size.y % block_dim.y), 1u) : 0u,
		dim >= 3 && global_work_size.z > 0 ? std::min(uint32_t(global_work_size.z % block_dim.z), 1u) : 0u
	};
	uint3 grid_dim { (global_work_size / block_dim) + grid_dim_overflow };
	grid_dim.max(1u);
	return { grid_dim, block_dim };
}

void metal_kernel::execute(const compute_queue& cqueue,
						   const bool& is_cooperative,
						   const bool& wait_until_completion,
						   const uint32_t& dim,
						   const uint3& global_work_size,
						   const uint3& local_work_size,
						   const vector<compute_kernel_arg>& args,
						   const vector<const compute_fence*>& wait_fences,
						   const vector<const compute_fence*>& signal_fences,
						   const char* debug_label,
						   kernel_completion_handler_f&& completion_handler) const {
	const auto dev = &cqueue.get_device();
	const auto ctx = (const metal_compute*)dev->context;
	
	// no cooperative support yet
	if (is_cooperative) {
		log_error("cooperative kernel execution is not supported for Metal");
		return;
	}
	
	// find entry for queue device
	const auto kernel_iter = get_kernel(cqueue);
	if(kernel_iter == kernels.cend()) {
		log_error("no kernel for this compute queue/device exists!");
		return;
	}
	
	
	//
	auto encoder = create_encoder(cqueue, kernel_iter->second, debug_label);
	for (const auto& fence : wait_fences) {
		[encoder->encoder waitForFence:((const metal_fence*)fence)->get_metal_fence()];
	}
	
	// create implicit args
	vector<compute_kernel_arg> implicit_args;

	// create + init printf buffer if this function uses soft-printf
	pair<compute_buffer*, uint32_t> printf_buffer_rsrc;
	if (llvm_toolchain::has_flag<llvm_toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(kernel_iter->second.info->flags)) {
		printf_buffer_rsrc = ctx->acquire_soft_printf_buffer(*dev);
		if (printf_buffer_rsrc.first) {
			initialize_printf_buffer(cqueue, *printf_buffer_rsrc.first);
			implicit_args.emplace_back(*printf_buffer_rsrc.first);
		}
	}

	// set and handle kernel arguments
	const kernel_entry& entry = kernel_iter->second;
	metal_args::set_and_handle_arguments<metal_args::ENCODER_TYPE::COMPUTE>(*dev, encoder->encoder, { entry.info }, args, implicit_args);
	
	// compute sizes
	auto [grid_dim, block_dim] = compute_grid_and_block_dim(kernel_iter->second, dim, global_work_size, local_work_size);
	const MTLSize metal_grid_dim { grid_dim.x, grid_dim.y, grid_dim.z };
	const MTLSize metal_block_dim { block_dim.x, block_dim.y, block_dim.z };
	
	// run
	[encoder->encoder dispatchThreadgroups:metal_grid_dim threadsPerThreadgroup:metal_block_dim];
	for (const auto& fence : signal_fences) {
		[encoder->encoder updateFence:((const metal_fence*)fence)->get_metal_fence()];
	}
	[encoder->encoder endEncoding];
	
	// if soft-printf is being used, block/wait for completion here and read-back results
	if (llvm_toolchain::has_flag<llvm_toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(kernel_iter->second.info->flags)) {
		auto internal_dev_queue = ctx->get_device_default_queue(*dev);
		[encoder->cmd_buffer addCompletedHandler:^(id <MTLCommandBuffer>) {
			auto cpu_printf_buffer = make_unique<uint32_t[]>(printf_buffer_size / 4);
			printf_buffer_rsrc.first->read(*internal_dev_queue, cpu_printf_buffer.get());
			handle_printf_buffer(cpu_printf_buffer);
			ctx->release_soft_printf_buffer(*dev, printf_buffer_rsrc);
		}];
	}
	
	if (completion_handler) {
		auto local_completion_handler = move(completion_handler);
		[encoder->cmd_buffer addCompletedHandler:^(id <MTLCommandBuffer>) {
			local_completion_handler();
		}];
	}

	[encoder->cmd_buffer commit];
	
	if (wait_until_completion) {
		[encoder->cmd_buffer waitUntilCompleted];
	}
}

typename metal_kernel::kernel_map_type::const_iterator metal_kernel::get_kernel(const compute_queue& cqueue) const {
	return kernels.find((const metal_device&)cqueue.get_device());
}

const compute_kernel::kernel_entry* metal_kernel::get_kernel_entry(const compute_device& dev) const {
	const auto ret = kernels.get((const metal_device&)dev);
	return !ret.first ? nullptr : &ret.second->second;
}

static const char* metal_data_type_to_string(const MTLDataType& data_type) {
	switch (data_type) {
		case MTLDataTypeNone: return "None";
		case MTLDataTypeStruct: return "Struct";
		case MTLDataTypeArray: return "Array";
		case MTLDataTypeFloat: return "Float";
		case MTLDataTypeFloat2: return "Float2";
		case MTLDataTypeFloat3: return "Float3";
		case MTLDataTypeFloat4: return "Float4";
		case MTLDataTypeFloat2x2: return "Float2x2";
		case MTLDataTypeFloat2x3: return "Float2x3";
		case MTLDataTypeFloat2x4: return "Float2x4";
		case MTLDataTypeFloat3x2: return "Float3x2";
		case MTLDataTypeFloat3x3: return "Float3x3";
		case MTLDataTypeFloat3x4: return "Float3x4";
		case MTLDataTypeFloat4x2: return "Float4x2";
		case MTLDataTypeFloat4x3: return "Float4x3";
		case MTLDataTypeFloat4x4: return "Float4x4";
		case MTLDataTypeHalf: return "Half";
		case MTLDataTypeHalf2: return "Half2";
		case MTLDataTypeHalf3: return "Half3";
		case MTLDataTypeHalf4: return "Half4";
		case MTLDataTypeHalf2x2: return "Half2x2";
		case MTLDataTypeHalf2x3: return "Half2x3";
		case MTLDataTypeHalf2x4: return "Half2x4";
		case MTLDataTypeHalf3x2: return "Half3x2";
		case MTLDataTypeHalf3x3: return "Half3x3";
		case MTLDataTypeHalf3x4: return "Half3x4";
		case MTLDataTypeHalf4x2: return "Half4x2";
		case MTLDataTypeHalf4x3: return "Half4x3";
		case MTLDataTypeHalf4x4: return "Half4x4";
		case MTLDataTypeInt: return "Int";
		case MTLDataTypeInt2: return "Int2";
		case MTLDataTypeInt3: return "Int3";
		case MTLDataTypeInt4: return "Int4";
		case MTLDataTypeUInt: return "UInt";
		case MTLDataTypeUInt2: return "UInt2";
		case MTLDataTypeUInt3: return "UInt3";
		case MTLDataTypeUInt4: return "UInt4";
		case MTLDataTypeShort: return "Short";
		case MTLDataTypeShort2: return "Short2";
		case MTLDataTypeShort3: return "Short3";
		case MTLDataTypeShort4: return "Short4";
		case MTLDataTypeUShort: return "UShort";
		case MTLDataTypeUShort2: return "UShort2";
		case MTLDataTypeUShort3: return "UShort3";
		case MTLDataTypeUShort4: return "UShort4";
		case MTLDataTypeChar: return "Char";
		case MTLDataTypeChar2: return "Char2";
		case MTLDataTypeChar3: return "Char3";
		case MTLDataTypeChar4: return "Char4";
		case MTLDataTypeUChar: return "UChar";
		case MTLDataTypeUChar2: return "UChar2";
		case MTLDataTypeUChar3: return "UChar3";
		case MTLDataTypeUChar4: return "UChar4";
		case MTLDataTypeBool: return "Bool";
		case MTLDataTypeBool2: return "Bool2";
		case MTLDataTypeBool3: return "Bool3";
		case MTLDataTypeBool4: return "Bool4";
		case MTLDataTypeTexture: return "Texture";
		case MTLDataTypeSampler: return "Sampler";
		case MTLDataTypePointer: return "Pointer";
		case MTLDataTypeR8Unorm: return "R8Unorm";
		case MTLDataTypeR8Snorm: return "R8Snorm";
		case MTLDataTypeR16Unorm: return "R16Unorm";
		case MTLDataTypeR16Snorm: return "R16Snorm";
		case MTLDataTypeRG8Unorm: return "RG8Unorm";
		case MTLDataTypeRG8Snorm: return "RG8Snorm";
		case MTLDataTypeRG16Unorm: return "RG16Unorm";
		case MTLDataTypeRG16Snorm: return "RG16Snorm";
		case MTLDataTypeRGBA8Unorm: return "RGBA8Unorm";
		case MTLDataTypeRGBA8Unorm_sRGB: return "RGBA8Unorm_sRGB";
		case MTLDataTypeRGBA8Snorm: return "RGBA8Snorm";
		case MTLDataTypeRGBA16Unorm: return "RGBA16Unorm";
		case MTLDataTypeRGBA16Snorm: return "RGBA16Snorm";
		case MTLDataTypeRGB10A2Unorm: return "RGB10A2Unorm";
		case MTLDataTypeRG11B10Float: return "RG11B10Float";
		case MTLDataTypeRGB9E5Float: return "RGB9E5Float";
		case MTLDataTypeRenderPipeline: return "RenderPipeline";
		case MTLDataTypeComputePipeline: return "ComputePipeline";
		case MTLDataTypeIndirectCommandBuffer: return "IndirectCommandBuffer";
#if (defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED >= 120000) || \
	(defined(__IPHONE_OS_VERSION_MAX_ALLOWED) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 150000)
		case MTLDataTypeLong: return "Long";
		case MTLDataTypeLong2: return "Long2";
		case MTLDataTypeLong3: return "Long3";
		case MTLDataTypeLong4: return "Long4";
		case MTLDataTypeULong: return "ULong";
		case MTLDataTypeULong2: return "ULong2";
		case MTLDataTypeULong3: return "ULong3";
		case MTLDataTypeULong4: return "ULong4";
#endif
		case MTLDataTypeVisibleFunctionTable: return "VisibleFunctionTable";
		case MTLDataTypeIntersectionFunctionTable: return "IntersectionFunctionTable";
		case MTLDataTypePrimitiveAccelerationStructure: return "PrimitiveAccelerationStructure";
		case MTLDataTypeInstanceAccelerationStructure: return "InstanceAccelerationStructure";
	}
	return "<unknown>";
}

static void dump_refl_array(MTLArrayType* arr_type, stringstream& sstr, const uint32_t level);
static void dump_refl_struct(MTLStructType* struct_type, stringstream& sstr, const uint32_t level);
static void dump_refl_struct_member(MTLStructMember* member_type, stringstream& sstr, const uint32_t level);
static void dump_refl_ptr(MTLPointerType* ptr_type, stringstream& sstr, const uint32_t level);

[[maybe_unused]] void dump_refl_array(MTLArrayType* arr_type, stringstream& sstr, const uint32_t level) {
	const string indent(level * 2, ' ');
	sstr << indent << "<array>" << endl;
	sstr << indent << "element type: " << metal_data_type_to_string([arr_type elementType]) << endl;
	sstr << indent << "length: " << [arr_type arrayLength] << endl;
	sstr << indent << "stride: " << [arr_type stride] << endl;
	sstr << indent << "argumentIndexStride: " << [arr_type argumentIndexStride] << endl;
	if ([arr_type elementStructType]) {
		dump_refl_struct([arr_type elementStructType], sstr, level + 1);
	}
	if ([arr_type elementArrayType]) {
		dump_refl_array([arr_type elementArrayType], sstr, level + 1);
	}
	if ([arr_type elementPointerType]) {
		dump_refl_ptr([arr_type elementPointerType], sstr, level + 1);
	}
}

[[maybe_unused]] void dump_refl_struct(MTLStructType* struct_type, stringstream& sstr, const uint32_t level) {
	const string indent(level * 2, ' ');
	sstr << indent << "<struct>" << endl;
	sstr << indent << "#members: " << [struct_type members].count << endl;
	for (MTLStructMember* member in [struct_type members]) {
		dump_refl_struct_member(member, sstr, level + 1);
	}
}

[[maybe_unused]] void dump_refl_struct_member(MTLStructMember* member_type, stringstream& sstr, const uint32_t level) {
	const string indent(level * 2, ' ');
	sstr << indent << "<member> " << endl;
	sstr << indent << "name: " << [[member_type name] UTF8String] << endl;
	sstr << indent << "type: " << metal_data_type_to_string([member_type dataType]) << endl;
	sstr << indent << "offset: " << ([member_type offset] != ~0ull ? to_string([member_type offset]) : "none") << endl;
	sstr << indent << "arg index: " << [member_type argumentIndex] << endl;
	if ([member_type structType]) {
		dump_refl_struct([member_type structType], sstr, level + 1);
	}
	if ([member_type arrayType]) {
		dump_refl_array([member_type arrayType], sstr, level + 1);
	}
	if ([member_type pointerType]) {
		dump_refl_ptr([member_type pointerType], sstr, level + 1);
	}
}

[[maybe_unused]] void dump_refl_ptr(MTLPointerType* ptr_type, stringstream& sstr, const uint32_t level) {
	const string indent(level * 2, ' ');
	sstr << indent << "<pointer>" << endl;
	sstr << indent << "element type: " << metal_data_type_to_string([ptr_type elementType]) << endl;
	sstr << indent << "size: " << [ptr_type dataSize] << endl;
	sstr << indent << "alignment: " << [ptr_type alignment] << endl;
	sstr << indent << "is arg buffer?: " << ([ptr_type elementIsArgumentBuffer] ? "yes" : "no") << endl;
	if ([ptr_type elementStructType]) {
		dump_refl_struct([ptr_type elementStructType], sstr, level + 1);
	}
	if ([ptr_type elementArrayType]) {
		dump_refl_array([ptr_type elementArrayType], sstr, level + 1);
	}
}

[[maybe_unused]] static void dump_refl_arg(const MTLAutoreleasedArgument& arg, stringstream& sstr) {
	sstr << "<argument>" << endl;
	sstr << "name: " << [[arg name] UTF8String] << endl;
	sstr << "index: " << [arg index] << endl;
	sstr << "buffer: size: " << [arg bufferDataSize] << ", alignment: " << [arg bufferAlignment] << ", " << metal_data_type_to_string([arg bufferDataType]) << endl;
	if ([arg bufferStructType]) {
		dump_refl_struct([arg bufferStructType], sstr, 1);
	} else if ([arg bufferPointerType]) {
		dump_refl_ptr([arg bufferPointerType], sstr, 1);
	}
}

unique_ptr<argument_buffer> metal_kernel::create_argument_buffer_internal(const compute_queue& cqueue,
																		  const kernel_entry& entry,
																		  const llvm_toolchain::arg_info& arg floor_unused,
																		  const uint32_t& user_arg_index,
																		  const uint32_t& ll_arg_index,
																		  const COMPUTE_MEMORY_FLAG& add_mem_flags) const {
	const auto& dev = cqueue.get_device();
	const auto& mtl_entry = (const metal_kernel_entry&)entry;
	auto mtl_func = (__bridge id<MTLFunction>)mtl_entry.kernel;
	
	// check if info exists
	const auto& arg_info = mtl_entry.info->args[ll_arg_index].argument_buffer_info;
	if (!arg_info) {
		log_error("no argument buffer info for arg at index #$", user_arg_index);
		return {};
	}
	
	// find the metal buffer index
	uint32_t buffer_idx = 0;
	for (uint32_t i = 0, count = uint32_t(mtl_entry.info->args.size()); i < min(ll_arg_index, count); ++i) {
		if (mtl_entry.info->args[i].special_type == SPECIAL_TYPE::STAGE_INPUT) {
			// only tessellation evaluation shaders may contain buffers in stage_input
			if (mtl_entry.info->type == llvm_toolchain::FUNCTION_TYPE::TESSELLATION_EVALUATION) {
				buffer_idx += mtl_entry.info->args[i].size;
			}
		} else if (mtl_entry.info->args[i].image_type == ARG_IMAGE_TYPE::NONE) {
			// all args except for images are buffers
			++buffer_idx;
		}
	}
	
	// create a dummy encoder so that we can retrieve the necessary buffer length (and do some validity checking)
	MTLAutoreleasedArgument reflection_data { nil };
	id <MTLArgumentEncoder> arg_encoder = [mtl_func newArgumentEncoderWithBufferIndex:buffer_idx
																		   reflection:&reflection_data];
	if (!arg_encoder) {
		log_error("failed to create argument encoder");
		return {};
	}
	
	const auto arg_buffer_size = (uint64_t)[arg_encoder encodedLength];
	if (arg_buffer_size == 0) {
		log_error("computed argument buffer size is 0");
		return {};
	}
	// round up to next multiple of page size
	const auto arg_buffer_size_page = arg_buffer_size + (arg_buffer_size % aligned_ptr<int>::page_size != 0u ?
														 (aligned_ptr<int>::page_size - (arg_buffer_size % aligned_ptr<int>::page_size)) : 0u);
	
	// debug dump of the arg buffer layout
#if 0
	if (reflection_data) {
		stringstream sstr;
		dump_refl_arg(reflection_data, sstr);
		log_undecorated("reflection data:\n$", sstr.str());
	}
#endif
	
	// figure out the top level arg indices (we don't need to go deeper, since non-constant/buffer vars in nested structs are not supported right now)
	vector<uint32_t> arg_indices;
	if (reflection_data && [reflection_data bufferStructType]) {
		MTLStructType* top_level_struct = [reflection_data bufferStructType];
		for (MTLStructMember* member in [top_level_struct members]) {
			arg_indices.emplace_back([member argumentIndex]);
		}
		if (arg_indices.empty()) {
			log_warn("no members in struct of arg buffer - falling back to simple indices (this might not work)");
		}
	} else {
		log_warn("invalid arg buffer reflection data - falling back to simple indices (this might not work)");
	}
	
	// create the argument buffer
	// NOTE: the buffer has to be allocated in managed mode (macOS) or shared mode (iOS) -> set appropriate flags
	auto storage_buffer_backing = make_aligned_ptr<uint8_t>(arg_buffer_size_page);
	memset(storage_buffer_backing.get(), 0, arg_buffer_size_page);
	auto buf = dev.context->create_buffer(cqueue, arg_buffer_size_page, storage_buffer_backing.get(),
										  COMPUTE_MEMORY_FLAG::READ |
										  COMPUTE_MEMORY_FLAG::HOST_WRITE |
										  COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY |
										  add_mem_flags);
	buf->set_debug_label(entry.info->name + "_arg_buffer");
	return make_unique<metal_argument_buffer>(*this, buf, move(storage_buffer_backing), arg_encoder, *arg_info, move(arg_indices));
}

#endif
