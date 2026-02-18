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

#include <floor/device/metal/metal_program.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/device/metal/metal_function.hpp>
#include <floor/device/metal/metal_shader.hpp>
#include <floor/device/metal/metal_device.hpp>
#include <floor/device/metal/metal_args.hpp>
#include <floor/device/toolchain.hpp>
#include <floor/floor.hpp>

namespace fl {

metal_program::metal_program(program_map_type&& programs_) :
device_program(retrieve_unique_function_names(programs_)), programs(std::move(programs_)) {
	if (programs.empty()) {
		return;
	}
	
	@autoreleasepool {
		// create all functions of all device programs
		// note that this essentially reshuffles the program "device -> functions" data to "functions -> devices"
		static const bool dump_reflection_info = floor::get_metal_dump_reflection_info();
		functions.reserve(function_names.size());
		for (const auto& function_name : function_names) {
			metal_function::function_map_type function_map;
			bool is_kernel = true;
			for (auto&& prog : programs) {
				if (!prog.second.valid) continue;
				for (const auto& info : prog.second.functions) {
					if (info.name == function_name) {
						if (should_ignore_function_for_device(*prog.first, info)) {
							continue;
						}
						
						metal_function::metal_function_entry entry;
						entry.info = &info;
						if (entry.info->type != toolchain::FUNCTION_TYPE::KERNEL) {
							is_kernel = false;
						}
						if (entry.info->has_valid_required_local_size()) {
							entry.max_local_size = info.required_local_size;
							entry.max_total_local_size = info.required_local_size.extent();
						} else {
							entry.max_local_size = prog.first->max_local_size;
							entry.max_total_local_size = prog.first->max_total_local_size;
						}
						if (entry.info->has_valid_required_simd_width()) {
							entry.required_simd_width = info.required_simd_width;
						}
						
						//
						const auto func_name = [NSString stringWithUTF8String:info.name.c_str()];
						if (!func_name) {
							log_error("invalid function name: $", info.name);
							continue;
						}
						id <MTLFunction> func = [prog.second.program newFunctionWithName:floor_force_nonnull(func_name)];
						if(!func) {
							log_error("failed to get function \"$\" for device \"$\"", info.name, prog.first->name);
							continue;
						}
						
						NSError* err = nullptr;
						id <MTLComputePipelineState> kernel_state = nil;
						bool supports_indirect_compute = false;
						if ([func functionType] == MTLFunctionTypeKernel) {
							MTLComputePipelineDescriptor* mtl_pipeline_desc = [[MTLComputePipelineDescriptor alloc] init];
							const std::string label = info.name + " pipeline";
							mtl_pipeline_desc.label = [NSString stringWithUTF8String:label.c_str()];
							mtl_pipeline_desc.computeFunction = func;
							
							// optimization opt-in
							mtl_pipeline_desc.threadGroupSizeIsMultipleOfThreadExecutionWidth = true;
							mtl_pipeline_desc.maxCallStackDepth = 0;
#if defined(__MAC_26_0) || defined(__IPHONE_26_0) || defined(__VISIONOS_26_0)
							if (entry.info->has_valid_required_local_size()) {
								if (@available(macOS 26.0, iOS 26.0, visionOS 26.0, *)) {
									mtl_pipeline_desc.requiredThreadsPerThreadgroup = {
										.width = entry.info->required_local_size.x,
										.height = entry.info->required_local_size.y,
										.depth = entry.info->required_local_size.z,
									};
								}
							}
#endif
							
							// implicitly support indirect compute when the function doesn't take any non-global-AS parameters
							bool has_non_global_args = false;
							for (const auto& func_arg : info.args) {
								if (func_arg.address_space != toolchain::ARG_ADDRESS_SPACE::GLOBAL &&
									!has_flag<toolchain::ARG_FLAG::ARGUMENT_BUFFER>(func_arg.flags)) {
									has_non_global_args = true;
									break;
								}
							}
							if (!has_non_global_args) {
								mtl_pipeline_desc.supportIndirectCommandBuffers = true;
							}
							
							// set buffer mutability
							metal_args::set_buffer_mutability<metal_args::ENCODER_TYPE::COMPUTE>(mtl_pipeline_desc, { &info });
							
							if (dump_reflection_info) {
								MTLAutoreleasedComputePipelineReflection refl_data { nil };
								kernel_state = [[prog.second.program device] newComputePipelineStateWithDescriptor:mtl_pipeline_desc
																										   options:(MTLPipelineOptionBindingInfo |
																													MTLPipelineOptionBufferTypeInfo)
																										reflection:&refl_data
																											 error:&err];
								if (refl_data) {
									dump_bindings_reflection("kernel \"" + function_name + "\"", [refl_data bindings]);
								}
							} else {
								kernel_state = [[prog.second.program device] newComputePipelineStateWithDescriptor:mtl_pipeline_desc
																										   options:MTLPipelineOptionNone
																										reflection:nil
																											 error:&err];
							}
							if (!kernel_state) {
								log_error("failed to create kernel state \"$\" for device \"$\": $", info.name, prog.first->name,
										  (err != nullptr ? [[err localizedDescription] UTF8String] : "unknown error"));
								continue;
							}
							supports_indirect_compute = [kernel_state supportIndirectCommandBuffers];
#if defined(FLOOR_DEBUG) || defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
							log_debug("$ ($): max work-items: $, simd width: $, local mem: $, indirect: $",
									  info.name, prog.first->name,
									  [kernel_state maxTotalThreadsPerThreadgroup], [kernel_state threadExecutionWidth],
									  [kernel_state staticThreadgroupMemoryLength], supports_indirect_compute ? "yes" : "no");
#endif
						}
						
						// success, insert necessary info/data everywhere
						prog.second.metal_functions.emplace_back(metal_program_entry::metal_function_data { func, kernel_state });
						entry.function = (__bridge void*)func;
						entry.kernel_state = (__bridge void*)kernel_state;
						entry.supports_indirect_compute = supports_indirect_compute;
						if (kernel_state != nil) {
							// adjust downwards from reported max local size
							entry.max_total_local_size = std::min(uint32_t([kernel_state maxTotalThreadsPerThreadgroup]),
																  entry.max_total_local_size);
							if (entry.max_total_local_size < entry.max_local_size.extent()) {
								entry.max_local_size = device_function::check_local_work_size(entry.max_local_size, entry.max_local_size,
																							  entry.max_total_local_size);
							}
						}
						function_map.insert_or_assign(prog.first, entry);
						break;
					}
				}
			}
			
			functions.emplace_back(is_kernel ?
								 std::make_shared<metal_function>(function_name, std::move(function_map)) :
								 std::make_shared<metal_shader>(std::move(function_map)));
		}
	}
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
		case MTLDataTypeLong: return "Long";
		case MTLDataTypeLong2: return "Long2";
		case MTLDataTypeLong3: return "Long3";
		case MTLDataTypeLong4: return "Long4";
		case MTLDataTypeULong: return "ULong";
		case MTLDataTypeULong2: return "ULong2";
		case MTLDataTypeULong3: return "ULong3";
		case MTLDataTypeULong4: return "ULong4";
		case MTLDataTypeVisibleFunctionTable: return "VisibleFunctionTable";
		case MTLDataTypeIntersectionFunctionTable: return "IntersectionFunctionTable";
		case MTLDataTypePrimitiveAccelerationStructure: return "PrimitiveAccelerationStructure";
		case MTLDataTypeInstanceAccelerationStructure: return "InstanceAccelerationStructure";
		case MTLDataTypeBFloat: return "BFloat";
		case MTLDataTypeBFloat2: return "BFloat2";
		case MTLDataTypeBFloat3: return "BFloat3";
		case MTLDataTypeBFloat4: return "BFloat4";
#if defined(__MAC_26_0) || defined(__IPHONE_26_0) || defined(__VISIONOS_26_0)
		case MTLDataTypeDepthStencilState: return "DepthStencilState";
		case MTLDataTypeTensor: return "Tensor";
#endif
	}
	return "<unknown>";
}

#if defined(__MAC_26_0) || defined(__IPHONE_26_0) || defined(__VISIONOS_26_0)
static const char* metal_tensor_data_type_to_string(const MTLTensorDataType& data_type) {
	switch (data_type) {
		case MTLTensorDataTypeNone: return "None";
		case MTLTensorDataTypeFloat32: return metal_data_type_to_string(MTLDataTypeFloat);
		case MTLTensorDataTypeFloat16: return metal_data_type_to_string(MTLDataTypeHalf);
		case MTLTensorDataTypeBFloat16: return metal_data_type_to_string(MTLDataTypeBFloat);
		case MTLTensorDataTypeInt8: return metal_data_type_to_string(MTLDataTypeChar);
		case MTLTensorDataTypeUInt8: return metal_data_type_to_string(MTLDataTypeUChar);
		case MTLTensorDataTypeInt16: return metal_data_type_to_string(MTLDataTypeShort);
		case MTLTensorDataTypeUInt16: return metal_data_type_to_string(MTLDataTypeUShort);
		case MTLTensorDataTypeInt32: return metal_data_type_to_string(MTLDataTypeInt);
		case MTLTensorDataTypeUInt32: return metal_data_type_to_string(MTLDataTypeUInt);
#if defined(__MAC_26_4) || defined(__IPHONE_26_4) || defined(__VISIONOS_26_4)
		case MTLTensorDataTypeInt4: return "TensorInt4";
		case MTLTensorDataTypeUInt4: return "TensorUInt4";
#endif
	}
	return "<unknown>";
}
#endif

static void dump_refl_array(MTLArrayType* arr_type, std::stringstream& sstr, const uint32_t level);
static void dump_refl_struct(MTLStructType* struct_type, std::stringstream& sstr, const uint32_t level);
static void dump_refl_struct_member(MTLStructMember* member_type, std::stringstream& sstr, const uint32_t level);
static void dump_refl_ptr(MTLPointerType* ptr_type, std::stringstream& sstr, const uint32_t level);

void dump_refl_array(MTLArrayType* arr_type, std::stringstream& sstr, const uint32_t level) {
	const std::string indent(level * 2, ' ');
	sstr << indent << "<array>" << std::endl;
	sstr << indent << "element type: " << metal_data_type_to_string([arr_type elementType]) << std::endl;
	sstr << indent << "length: " << [arr_type arrayLength] << std::endl;
	sstr << indent << "stride: " << [arr_type stride] << std::endl;
	sstr << indent << "argumentIndexStride: " << [arr_type argumentIndexStride] << std::endl;
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

void dump_refl_struct(MTLStructType* struct_type, std::stringstream& sstr, const uint32_t level) {
	const std::string indent(level * 2, ' ');
	sstr << indent << "<struct>" << std::endl;
	sstr << indent << "#members: " << [struct_type members].count << std::endl;
	for (MTLStructMember* member in [struct_type members]) {
		dump_refl_struct_member(member, sstr, level + 1);
	}
}

void dump_refl_struct_member(MTLStructMember* member_type, std::stringstream& sstr, const uint32_t level) {
	const std::string indent(level * 2, ' ');
	sstr << indent << "<member> " << std::endl;
	sstr << indent << "name: " << [[member_type name] UTF8String] << std::endl;
	sstr << indent << "type: " << metal_data_type_to_string([member_type dataType]) << std::endl;
	sstr << indent << "offset: " << ([member_type offset] != ~0ull ? std::to_string([member_type offset]) : "none") << std::endl;
	sstr << indent << "arg index: " << [member_type argumentIndex] << std::endl;
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

void dump_refl_ptr(MTLPointerType* ptr_type, std::stringstream& sstr, const uint32_t level) {
	const std::string indent(level * 2, ' ');
	sstr << indent << "<pointer>" << std::endl;
	sstr << indent << "element type: " << metal_data_type_to_string([ptr_type elementType]) << std::endl;
	sstr << indent << "size: " << [ptr_type dataSize] << std::endl;
	sstr << indent << "alignment: " << [ptr_type alignment] << std::endl;
	sstr << indent << "is arg buffer?: " << ([ptr_type elementIsArgumentBuffer] ? "yes" : "no") << std::endl;
	if ([ptr_type elementStructType]) {
		dump_refl_struct([ptr_type elementStructType], sstr, level + 1);
	}
	if ([ptr_type elementArrayType]) {
		dump_refl_array([ptr_type elementArrayType], sstr, level + 1);
	}
}

void metal_program::dump_bindings_reflection(const std::string& reflection_info_name, NSArray<id<MTLBinding>>* bindings) {
	if (!bindings) {
		return;
	}
	
	std::stringstream sstr;
	const std::string indent(2, ' ');
	for (id<MTLBinding> binding in bindings) {
		sstr << indent << "binding \"" << [[binding name] UTF8String] << "\" @" << [binding index] << ":";
		switch ([binding access]) {
			case MTLBindingAccessReadOnly:
				sstr << " read-only";
				break;
			case MTLBindingAccessWriteOnly:
				sstr << " write-only";
				break;
			case MTLBindingAccessReadWrite:
				sstr << " read-write";
				break;
		}
		if (![binding isUsed]) {
			sstr << ", unused";
		}
		if ([binding isArgument]) {
			sstr << ", is-argument";
		}
		switch ([binding type]) {
			case MTLBindingTypeBuffer: {
				auto buffer_binding = (id<MTLBufferBinding>)binding;
				sstr << ", buffer: size: " << [buffer_binding bufferDataSize];
				sstr << ", alignment: " << [buffer_binding bufferAlignment];
				sstr << ", type: " << metal_data_type_to_string([buffer_binding bufferDataType]);
				sstr << std::endl;
				if ([buffer_binding bufferStructType]) {
					dump_refl_struct([buffer_binding bufferStructType], sstr, 2);
				} else if ([buffer_binding bufferPointerType]) {
					dump_refl_ptr([buffer_binding bufferPointerType], sstr, 2);
				}
				break;
			}
			case MTLBindingTypeThreadgroupMemory: {
				auto threadgroup_binding = (id<MTLThreadgroupBinding>)binding;
				sstr << ", threadgroup-memory: size: " << [threadgroup_binding threadgroupMemoryDataSize];
				sstr << ", alignment: " << [threadgroup_binding threadgroupMemoryAlignment];
				sstr << std::endl;
				break;
			}
			case MTLBindingTypeTexture: {
				auto tex_binding = (id<MTLTextureBinding>)binding;
				sstr << ", image: ";
				switch ([tex_binding textureType]) {
					case MTLTextureType1D:
						sstr << "1D";
						break;
					case MTLTextureType1DArray:
						sstr << "1D-array";
						break;
					case MTLTextureType2D:
						sstr << "2D";
						break;
					case MTLTextureType2DArray:
						sstr << "2D-array";
						break;
					case MTLTextureType2DMultisample:
						sstr << "2D-msaa";
						break;
					case MTLTextureType3D:
						sstr << "3D";
						break;
					case MTLTextureTypeCube:
						sstr << "cube";
						break;
					case MTLTextureTypeCubeArray:
						sstr << "cube-array";
						break;
					case MTLTextureType2DMultisampleArray:
						sstr << "2D-msaa-array";
						break;
					case MTLTextureTypeTextureBuffer:
						sstr << "texture-buffer";
						break;
				}
				if ([tex_binding isDepthTexture]) {
					sstr << "-depth";
				}
				sstr << ", type: " << metal_data_type_to_string([tex_binding textureDataType]);
				sstr << ", array-length: " << [tex_binding arrayLength];
				sstr << std::endl;
				break;
			}
			case MTLBindingTypeObjectPayload: {
				auto obj_payload_binding = (id<MTLObjectPayloadBinding>)binding;
				sstr << "object-payload: size: " << [obj_payload_binding objectPayloadDataSize];
				sstr << ", alignment: " << [obj_payload_binding objectPayloadAlignment];
				sstr << std::endl;
				break;
			}
#if defined(__MAC_26_0) || defined(__IPHONE_26_0) || defined(__VISIONOS_26_0)
			case MTLBindingTypeTensor: {
				auto tensor_binding = (id<MTLTensorBinding>)binding;
				sstr << "tensor: type: " << metal_tensor_data_type_to_string([tensor_binding tensorDataType]);
				sstr << ", index type: " << metal_data_type_to_string([tensor_binding indexType]);
				if (auto dims = [tensor_binding dimensions]; dims != nil) {
					sstr << ", dim (";
					for (uint32_t dim_idx = 0, dim_count = std::min(uint32_t(dims.rank), uint32_t(MTL_TENSOR_MAX_RANK));
						 dim_idx < dim_count; ++dim_idx) {
						const auto extent = [dims extentAtDimensionIndex:dim_idx];
						if (extent < 0) {
							break;
						}
						sstr << extent;
					}
					sstr << ")";
				}
				sstr << std::endl;
				break;
			}
#endif
			case MTLBindingTypeSampler:
			case MTLBindingTypeImageblockData:
			case MTLBindingTypeImageblock:
			case MTLBindingTypeVisibleFunctionTable:
			case MTLBindingTypePrimitiveAccelerationStructure:
			case MTLBindingTypeInstanceAccelerationStructure:
			case MTLBindingTypeIntersectionFunctionTable:
				// no particular binding info
				break;
		}
	}
	
	log_undecorated("$ reflection info:\n$", reflection_info_name, sstr.str());
}

} // namespace fl

#endif
