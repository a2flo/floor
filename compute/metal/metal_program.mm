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

#include <floor/compute/metal/metal_program.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/compute/metal/metal_kernel.hpp>
#include <floor/graphics/metal/metal_shader.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/core/core.hpp>
#include <floor/floor/floor.hpp>

metal_program::metal_program(program_map_type&& programs_) :
compute_program(retrieve_unique_kernel_names(programs_)), programs(std::move(programs_)) {
	if (programs.empty()) {
		return;
	}
	
	@autoreleasepool {
		// create all kernels of all device programs
		// note that this essentially reshuffles the program "device -> kernels" data to "kernels -> devices"
		static const bool dump_reflection_info = floor::get_metal_dump_reflection_info();
		kernels.reserve(kernel_names.size());
		for(const auto& kernel_name : kernel_names) {
			metal_kernel::kernel_map_type kernel_map;
			bool is_kernel = true;
			for(auto& prog : programs) {
				if(!prog.second.valid) continue;
				for(const auto& info : prog.second.functions) {
					if(info.name == kernel_name) {
						metal_kernel::metal_kernel_entry entry;
						entry.info = &info;
						entry.max_local_size = prog.first->max_local_size;
						if (entry.info->type != llvm_toolchain::FUNCTION_TYPE::KERNEL) {
							is_kernel = false;
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
						if([func functionType] == MTLFunctionTypeKernel) {
							MTLComputePipelineDescriptor* mtl_pipeline_desc = [[MTLComputePipelineDescriptor alloc] init];
							const string label = info.name + " pipeline";
							mtl_pipeline_desc.label = [NSString stringWithUTF8String:label.c_str()];
							mtl_pipeline_desc.computeFunction = func;
							
							// optimization opt-in
							mtl_pipeline_desc.threadGroupSizeIsMultipleOfThreadExecutionWidth = true;
							
							// implicitly support indirect compute when the function doesn't take any image parameters
							bool has_image_args = false;
							for (const auto& func_arg : info.args) {
								if (func_arg.image_type != llvm_toolchain::ARG_IMAGE_TYPE::NONE) {
									has_image_args = true;
									break;
								}
							}
							if (!has_image_args) {
								mtl_pipeline_desc.supportIndirectCommandBuffers = true;
							}
							
							// TODO: set buffer mutability
							
							if (dump_reflection_info) {
								MTLAutoreleasedComputePipelineReflection refl_data { nil };
								kernel_state = [[prog.second.program device] newComputePipelineStateWithDescriptor:mtl_pipeline_desc
																										   options:(
#if defined(__MAC_15_0) || defined(__IPHONE_18_0)
																													MTLPipelineOptionBindingInfo |
#else
																													MTLPipelineOptionArgumentInfo |
#endif
																													MTLPipelineOptionBufferTypeInfo)
																										reflection:&refl_data
																											 error:&err];
								if (refl_data) {
									dump_bindings_reflection("kernel \"" + kernel_name + "\"", [refl_data bindings]);
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
							log_debug("$ ($): max work-items: $, simd width: $, local mem: $",
									  info.name, prog.first->name,
									  [kernel_state maxTotalThreadsPerThreadgroup], [kernel_state threadExecutionWidth], [kernel_state staticThreadgroupMemoryLength]);
#endif
						}
						
						// success, insert necessary info/data everywhere
						prog.second.metal_kernels.emplace_back(metal_program_entry::metal_kernel_data { func, kernel_state });
						entry.kernel = (__bridge void*)func;
						entry.kernel_state = (__bridge void*)kernel_state;
						entry.supports_indirect_compute = supports_indirect_compute;
						if(kernel_state != nil) {
							entry.max_total_local_size = (uint32_t)[kernel_state maxTotalThreadsPerThreadgroup];
						}
						kernel_map.insert_or_assign(prog.first, entry);
						break;
					}
				}
			}
			
			kernels.emplace_back(is_kernel ?
								 make_shared<metal_kernel>(kernel_name, std::move(kernel_map)) :
								 make_shared<metal_shader>(std::move(kernel_map)));
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
#if defined(__MAC_14_0) || defined(__IPHONE_17_0)
		case MTLDataTypeBFloat: return "BFloat";
		case MTLDataTypeBFloat2: return "BFloat2";
		case MTLDataTypeBFloat3: return "BFloat3";
		case MTLDataTypeBFloat4: return "BFloat4";
#endif
	}
	return "<unknown>";
}

static void dump_refl_array(MTLArrayType* arr_type, stringstream& sstr, const uint32_t level);
static void dump_refl_struct(MTLStructType* struct_type, stringstream& sstr, const uint32_t level);
static void dump_refl_struct_member(MTLStructMember* member_type, stringstream& sstr, const uint32_t level);
static void dump_refl_ptr(MTLPointerType* ptr_type, stringstream& sstr, const uint32_t level);

void dump_refl_array(MTLArrayType* arr_type, stringstream& sstr, const uint32_t level) {
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

void dump_refl_struct(MTLStructType* struct_type, stringstream& sstr, const uint32_t level) {
	const string indent(level * 2, ' ');
	sstr << indent << "<struct>" << endl;
	sstr << indent << "#members: " << [struct_type members].count << endl;
	for (MTLStructMember* member in [struct_type members]) {
		dump_refl_struct_member(member, sstr, level + 1);
	}
}

void dump_refl_struct_member(MTLStructMember* member_type, stringstream& sstr, const uint32_t level) {
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

void dump_refl_ptr(MTLPointerType* ptr_type, stringstream& sstr, const uint32_t level) {
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

void metal_program::dump_bindings_reflection(const string& reflection_info_name, NSArray<id<MTLBinding>>* bindings) {
	if (!bindings) {
		return;
	}
	
	stringstream sstr;
	const string indent(2, ' ');
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
				sstr << endl;
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
				sstr << endl;
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
				sstr << endl;
				break;
			}
			case MTLBindingTypeObjectPayload: {
				auto obj_payload_binding = (id<MTLObjectPayloadBinding>)binding;
				sstr << "object-payload: size: " << [obj_payload_binding objectPayloadDataSize];
				sstr << ", alignment: " << [obj_payload_binding objectPayloadAlignment];
				sstr << endl;
				break;
			}
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

#endif
