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

#include <floor/device/metal/metal_pipeline.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/core/essentials.hpp>
#include <floor/device/metal/metal_program.hpp>
#include <floor/device/metal/metal_function.hpp>
#include <floor/device/metal/metal_device.hpp>
#include <floor/device/metal/metal_image.hpp>
#include <floor/floor.hpp>

namespace fl {

metal_pipeline::metal_pipeline(const render_pipeline_description& pipeline_desc_,
							   const std::vector<std::unique_ptr<device>>& devices,
							   const bool with_multi_view_support) :
graphics_pipeline(pipeline_desc_, with_multi_view_support) {
	// NOTE: with Metal, we don't actually have to create an extra pipeline for multi-view support
	
	const auto mtl_vs = (const metal_function*)pipeline_desc.vertex_shader;
	const auto mtl_fs = (const metal_function*)pipeline_desc.fragment_shader;
	
	static const bool dump_reflection_info = floor::get_metal_dump_reflection_info();
	
	@autoreleasepool {
		for (const auto& dev : devices) {
			const auto& mtl_dev = ((const metal_device&)*dev).device;
			const auto mtl_vs_entry = (const metal_function::metal_function_entry*)mtl_vs->get_function_entry(*dev);
			const auto mtl_fs_entry = (mtl_fs != nullptr ? (const metal_function::metal_function_entry*)mtl_fs->get_function_entry(*dev) : nullptr);
			
			MTLRenderPipelineDescriptor* mtl_pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
			mtl_pipeline_desc.label = @"metal pipeline";
			// NOTE: there is no difference here between a pre- and post-tessallation vertex shader
			mtl_pipeline_desc.vertexFunction = (__bridge __unsafe_unretained id<MTLFunction>)mtl_vs_entry->function;
			mtl_pipeline_desc.fragmentFunction = (mtl_fs_entry != nullptr ?
												  (__bridge __unsafe_unretained id<MTLFunction>)mtl_fs_entry->function : nil);
			mtl_pipeline_desc.supportIndirectCommandBuffers = pipeline_desc.support_indirect_rendering;
			
			// multi-sampling
			if (pipeline_desc.sample_count > 1) {
				mtl_pipeline_desc.rasterSampleCount = pipeline_desc.sample_count;
			}
			
			// set color attachments
			for (size_t i = 0, count = pipeline_desc.color_attachments.size(); i < count; ++i) {
				const auto& color_att = pipeline_desc.color_attachments[i];
				if (color_att.format == IMAGE_TYPE::NONE) {
					log_error("color attachment image type must not be NONE!");
					return;
				}
				
				const auto metal_pixel_format = metal_image::metal_pixel_format_from_image_type(color_att.format);
				if (!metal_pixel_format) {
					log_error("no matching Metal pixel format found for color image type $X", color_att.format);
					return;
				}
				mtl_pipeline_desc.colorAttachments[i].pixelFormat = *metal_pixel_format;
				
				// handle blending
				if (color_att.blend.enable) {
					mtl_pipeline_desc.colorAttachments[i].blendingEnabled = true;
					mtl_pipeline_desc.colorAttachments[i].sourceRGBBlendFactor = metal_blend_factor_from_blend_factor(color_att.blend.src_color_factor);
					mtl_pipeline_desc.colorAttachments[i].sourceAlphaBlendFactor = metal_blend_factor_from_blend_factor(color_att.blend.src_alpha_factor);
					mtl_pipeline_desc.colorAttachments[i].destinationRGBBlendFactor = metal_blend_factor_from_blend_factor(color_att.blend.dst_color_factor);
					mtl_pipeline_desc.colorAttachments[i].destinationAlphaBlendFactor = metal_blend_factor_from_blend_factor(color_att.blend.dst_alpha_factor);
					mtl_pipeline_desc.colorAttachments[i].rgbBlendOperation = metal_blend_op_from_blend_op(color_att.blend.color_blend_op);
					mtl_pipeline_desc.colorAttachments[i].alphaBlendOperation = metal_blend_op_from_blend_op(color_att.blend.alpha_blend_op);
					
					if (!color_att.blend.write_mask.all()) {
						// actually need to compute/set a mask when not everything is written
						MTLColorWriteMask mask = MTLColorWriteMaskNone;
						if (color_att.blend.write_mask.x) {
							mask |= MTLColorWriteMaskRed;
						}
						if (color_att.blend.write_mask.y) {
							mask |= MTLColorWriteMaskGreen;
						}
						if (color_att.blend.write_mask.z) {
							mask |= MTLColorWriteMaskBlue;
						}
						if (color_att.blend.write_mask.w) {
							mask |= MTLColorWriteMaskAlpha;
						}
						mtl_pipeline_desc.colorAttachments[i].writeMask = mask;
					}
				} else {
					mtl_pipeline_desc.colorAttachments[i].blendingEnabled = false;
				}
			}
			
			// set optional depth attachment
			if (pipeline_desc.depth_attachment.format != IMAGE_TYPE::NONE) {
				const auto metal_pixel_format = metal_image::metal_pixel_format_from_image_type(pipeline_desc.depth_attachment.format);
				if (!metal_pixel_format) {
					log_error("no matching Metal pixel format found for depth image type $X", pipeline_desc.depth_attachment.format);
					return;
				}
				mtl_pipeline_desc.depthAttachmentPixelFormat = *metal_pixel_format;
			}
			
			// stencil is not supported yet
			mtl_pipeline_desc.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
			
			// set primitive type
			MTLPrimitiveTopologyClass primitive = MTLPrimitiveTopologyClass::MTLPrimitiveTopologyClassUnspecified;
			switch (pipeline_desc.primitive) {
				case PRIMITIVE::POINT:
					primitive = MTLPrimitiveTopologyClass::MTLPrimitiveTopologyClassPoint;
					break;
				case PRIMITIVE::LINE:
				case PRIMITIVE::LINE_STRIP:
					primitive = MTLPrimitiveTopologyClass::MTLPrimitiveTopologyClassLine;
					break;
				case PRIMITIVE::TRIANGLE:
				case PRIMITIVE::TRIANGLE_STRIP:
					primitive = MTLPrimitiveTopologyClass::MTLPrimitiveTopologyClassTriangle;
					break;
			}
			mtl_pipeline_desc.inputPrimitiveTopology = primitive;
			
			// tessellation
			MTLVertexDescriptor* mtl_vertex_desc = [MTLVertexDescriptor vertexDescriptor];
			if (pipeline_desc.tessellation.max_factor > 0u) {
				if (pipeline_desc.tessellation.max_factor > 64u) {
					log_error("max tessellation factor is out-of-range: $!", pipeline_desc.tessellation.max_factor);
					return;
				}
				if (pipeline_desc.primitive != PRIMITIVE::TRIANGLE) {
					log_error("when using tessellation, pipeline primitive type must be triangle");
					return;
				}
				mtl_pipeline_desc.maxTessellationFactor = pipeline_desc.tessellation.max_factor;
				mtl_pipeline_desc.tessellationFactorScaleEnabled = false; // never
				mtl_pipeline_desc.tessellationFactorFormat = MTLTessellationFactorFormatHalf;
				auto index_type = MTLTessellationControlPointIndexTypeNone;
				if (pipeline_desc.tessellation.is_indexed_draw) {
					switch (pipeline_desc.tessellation.index_type) {
						case INDEX_TYPE::UINT:
							index_type = MTLTessellationControlPointIndexTypeUInt32;
							break;
						case INDEX_TYPE::USHORT:
							index_type = MTLTessellationControlPointIndexTypeUInt16;
							break;
					}
				}
				mtl_pipeline_desc.tessellationControlPointIndexType = index_type;
				mtl_pipeline_desc.tessellationFactorStepFunction = MTLTessellationFactorStepFunctionPerPatch;
				mtl_pipeline_desc.tessellationPartitionMode = metal_tessellation_partition_mode_from_spacing(pipeline_desc.tessellation.spacing);
				mtl_pipeline_desc.tessellationOutputWindingOrder = metal_winding_from_winding(pipeline_desc.tessellation.winding);
				
				// when using tessellation, we *must* also specify the vertex attributes that are being used,
				// since vertices / control points will be fetched by fixed-function hardware and not a shader
				if (pipeline_desc.tessellation.vertex_attributes.empty()) {
					log_error("must specify vertex attributes when using tessellation");
					return;
				}
				uint32_t vattr_idx = 0;
				for (const auto& vattr : pipeline_desc.tessellation.vertex_attributes) {
					mtl_vertex_desc.attributes[vattr_idx].format = metal_vertex_format_from_vertex_format(vattr);
					if (mtl_vertex_desc.attributes[vattr_idx].format == MTLVertexFormatInvalid) {
						log_error("invalid or incompatible vertex attribute format: $X", vattr);
						return;
					}
					mtl_vertex_desc.attributes[vattr_idx].offset = 0u;
					mtl_vertex_desc.attributes[vattr_idx].bufferIndex = vattr_idx; // contiguous (ensured on the compiler side)
					mtl_vertex_desc.layouts[vattr_idx].stepFunction = MTLVertexStepFunctionPerPatchControlPoint;
					mtl_vertex_desc.layouts[vattr_idx].stepRate = 1u;
					mtl_vertex_desc.layouts[vattr_idx].stride = vertex_bytes(vattr);
					++vattr_idx;
				}
				mtl_pipeline_desc.vertexDescriptor = mtl_vertex_desc;
			}
			
			// TODO: set per-buffer mutability
			//mtl_pipeline_desc.vertexBuffers[0].mutability = MTLMutability::MTLMutabilityMutable;
			//mtl_pipeline_desc.fragmentBuffers[0].mutability = MTLMutability::MTLMutabilityImmutable;
			
			// finally create the pipeline object
			metal_pipeline_entry entry;
			NSError* error = nullptr;
			if (dump_reflection_info) {
				MTLAutoreleasedRenderPipelineReflection refl_data { nil };
				entry.pipeline_state = [mtl_dev newRenderPipelineStateWithDescriptor:mtl_pipeline_desc
																			 options:(
#if defined(__MAC_15_0) || defined(__IPHONE_18_0)
																					  MTLPipelineOptionBindingInfo |
#else
																					  MTLPipelineOptionArgumentInfo |
#endif
																					  MTLPipelineOptionBufferTypeInfo)
																		  reflection:&refl_data
																			   error:&error];
				if (refl_data) {
					metal_program::dump_bindings_reflection("vertex shader \"" + mtl_vs_entry->info->name + "\"", [refl_data vertexBindings]);
					if (mtl_fs_entry) {
						metal_program::dump_bindings_reflection("fragment shader \"" + mtl_vs_entry->info->name + "\"", [refl_data fragmentBindings]);
					}
				}
			} else {
				entry.pipeline_state = [mtl_dev newRenderPipelineStateWithDescriptor:mtl_pipeline_desc
																			   error:&error];
			}
			if (!entry.pipeline_state) {
				log_error("failed to create pipeline state for device $: $", dev->name,
						  (error != nullptr ? [[error localizedDescription] UTF8String] : "unknown error"));
				return;
			}
			entry.vs_entry = mtl_vs_entry;
			entry.fs_entry = mtl_fs_entry;
			
			// create depth/stencil state
			// TODO: depth range?
			auto depth_stencil_desc = [[MTLDepthStencilDescriptor alloc] init];
			depth_stencil_desc.depthWriteEnabled = pipeline_desc.depth.write;
			depth_stencil_desc.depthCompareFunction = metal_compare_func_from_depth_compare(pipeline_desc.depth.compare);
			entry.depth_stencil_state = [mtl_dev newDepthStencilStateWithDescriptor:depth_stencil_desc];
			if (!entry.depth_stencil_state) {
				log_error("failed to create depth/stencil state for device $", dev->name);
				return;
			}
			
			pipelines.insert(dev.get(), entry);
		}
	}
	
	// success
	valid = true;
}

metal_pipeline::~metal_pipeline() {
	@autoreleasepool {
		pipelines.clear();
	}
}

MTLPrimitiveType metal_pipeline::metal_primitive_type_from_primitive(const PRIMITIVE& primitive) {
	switch (primitive) {
		case PRIMITIVE::POINT:
			return MTLPrimitiveTypePoint;
		case PRIMITIVE::LINE:
			return MTLPrimitiveTypeLine;
		case PRIMITIVE::LINE_STRIP:
			return MTLPrimitiveTypeLineStrip;
		case PRIMITIVE::TRIANGLE:
			return MTLPrimitiveTypeTriangle;
		case PRIMITIVE::TRIANGLE_STRIP:
			return MTLPrimitiveTypeTriangleStrip;
	}
}

MTLCullMode metal_pipeline::metal_cull_mode_from_cull_mode(const CULL_MODE& cull_mode) {
	switch (cull_mode) {
		case CULL_MODE::NONE:
			return MTLCullModeNone;
		case CULL_MODE::BACK:
			return MTLCullModeBack;
		case CULL_MODE::FRONT:
			return MTLCullModeFront;
	}
}

MTLWinding metal_pipeline::metal_winding_from_front_face(const FRONT_FACE& front_face) {
	switch (front_face) {
		case FRONT_FACE::CLOCKWISE:
			return MTLWindingClockwise;
		case FRONT_FACE::COUNTER_CLOCKWISE:
			return MTLWindingCounterClockwise;
	}
}

MTLBlendFactor metal_pipeline::metal_blend_factor_from_blend_factor(const BLEND_FACTOR& blend_factor) {
	switch (blend_factor) {
		case BLEND_FACTOR::ZERO:
			return MTLBlendFactorZero;
		case BLEND_FACTOR::ONE:
			return MTLBlendFactorOne;
		
		case BLEND_FACTOR::SRC_COLOR:
			return MTLBlendFactorSourceColor;
		case BLEND_FACTOR::ONE_MINUS_SRC_COLOR:
			return MTLBlendFactorOneMinusSourceColor;
		case BLEND_FACTOR::DST_COLOR:
			return MTLBlendFactorDestinationColor;
		case BLEND_FACTOR::ONE_MINUS_DST_COLOR:
			return MTLBlendFactorOneMinusDestinationColor;
		
		case BLEND_FACTOR::SRC_ALPHA:
			return MTLBlendFactorSourceAlpha;
		case BLEND_FACTOR::ONE_MINUS_SRC_ALPHA:
			return MTLBlendFactorOneMinusSourceAlpha;
		case BLEND_FACTOR::DST_ALPHA:
			return MTLBlendFactorDestinationAlpha;
		case BLEND_FACTOR::ONE_MINUS_DST_ALPHA:
			return MTLBlendFactorOneMinusDestinationAlpha;
		case BLEND_FACTOR::SRC_ALPHA_SATURATE:
			return MTLBlendFactorSourceAlphaSaturated;
		
		case BLEND_FACTOR::BLEND_COLOR:
			return MTLBlendFactorBlendColor;
		case BLEND_FACTOR::ONE_MINUS_BLEND_COLOR:
			return MTLBlendFactorOneMinusBlendColor;
		case BLEND_FACTOR::BLEND_ALPHA:
			return MTLBlendFactorBlendAlpha;
		case BLEND_FACTOR::ONE_MINUE_BLEND_ALPHA:
			return MTLBlendFactorOneMinusBlendAlpha;
	}
}

MTLBlendOperation metal_pipeline::metal_blend_op_from_blend_op(const BLEND_OP& blend_op) {
	switch (blend_op) {
		case BLEND_OP::ADD:
			return MTLBlendOperationAdd;
		case BLEND_OP::SUB:
			return MTLBlendOperationSubtract;
		case BLEND_OP::REV_SUB:
			return MTLBlendOperationReverseSubtract;
		case BLEND_OP::MIN:
			return MTLBlendOperationMin;
		case BLEND_OP::MAX:
			return MTLBlendOperationMax;
	}
}

MTLCompareFunction metal_pipeline::metal_compare_func_from_depth_compare(const DEPTH_COMPARE& depth_compare) {
	switch (depth_compare) {
		case DEPTH_COMPARE::NEVER:
			return MTLCompareFunctionNever;
		case DEPTH_COMPARE::LESS:
			return MTLCompareFunctionLess;
		case DEPTH_COMPARE::EQUAL:
			return MTLCompareFunctionEqual;
		case DEPTH_COMPARE::LESS_OR_EQUAL:
			return MTLCompareFunctionLessEqual;
		case DEPTH_COMPARE::GREATER:
			return MTLCompareFunctionGreater;
		case DEPTH_COMPARE::NOT_EQUAL:
			return MTLCompareFunctionNotEqual;
		case DEPTH_COMPARE::GREATER_OR_EQUAL:
			return MTLCompareFunctionGreaterEqual;
		case DEPTH_COMPARE::ALWAYS:
			return MTLCompareFunctionAlways;
	}
}

MTLTessellationPartitionMode metal_pipeline::metal_tessellation_partition_mode_from_spacing(const TESSELLATION_SPACING& spacing) {
	switch (spacing) {
		case TESSELLATION_SPACING::EQUAL:
			return MTLTessellationPartitionModeInteger;
		case TESSELLATION_SPACING::FRACTIONAL_ODD:
			return MTLTessellationPartitionModeFractionalOdd;
		case TESSELLATION_SPACING::FRACTIONAL_EVEN:
			return MTLTessellationPartitionModeFractionalEven;
	}
}

MTLWinding metal_pipeline::metal_winding_from_winding(const TESSELLATION_WINDING& winding) {
	switch (winding) {
		case TESSELLATION_WINDING::CLOCKWISE:
			return MTLWindingClockwise;
		case TESSELLATION_WINDING::COUNTER_CLOCKWISE:
			return MTLWindingCounterClockwise;
	}
}

MTLVertexFormat metal_pipeline::metal_vertex_format_from_vertex_format(const VERTEX_FORMAT& vertex_format) {
	static const std::unordered_map<VERTEX_FORMAT, MTLVertexFormat> format_lut {
		{ VERTEX_FORMAT::HALF1, MTLVertexFormatHalf },
		{ VERTEX_FORMAT::HALF2, MTLVertexFormatHalf2 },
		{ VERTEX_FORMAT::HALF3, MTLVertexFormatHalf3 },
		{ VERTEX_FORMAT::HALF4, MTLVertexFormatHalf4 },
		{ VERTEX_FORMAT::FLOAT1, MTLVertexFormatFloat },
		{ VERTEX_FORMAT::FLOAT2, MTLVertexFormatFloat2 },
		{ VERTEX_FORMAT::FLOAT3, MTLVertexFormatFloat3 },
		{ VERTEX_FORMAT::FLOAT4, MTLVertexFormatFloat4 },
		
		{ VERTEX_FORMAT::UCHAR1, MTLVertexFormatUChar },
		{ VERTEX_FORMAT::UCHAR2, MTLVertexFormatUChar2 },
		{ VERTEX_FORMAT::UCHAR3, MTLVertexFormatUChar3 },
		{ VERTEX_FORMAT::UCHAR4, MTLVertexFormatUChar4 },
		{ VERTEX_FORMAT::USHORT1, MTLVertexFormatUShort },
		{ VERTEX_FORMAT::USHORT2, MTLVertexFormatUShort2 },
		{ VERTEX_FORMAT::USHORT3, MTLVertexFormatUShort3 },
		{ VERTEX_FORMAT::USHORT4, MTLVertexFormatUShort4 },
		{ VERTEX_FORMAT::UINT1, MTLVertexFormatUInt },
		{ VERTEX_FORMAT::UINT2, MTLVertexFormatUInt2 },
		{ VERTEX_FORMAT::UINT3, MTLVertexFormatUInt3 },
		{ VERTEX_FORMAT::UINT4, MTLVertexFormatUInt4 },
		
		{ VERTEX_FORMAT::CHAR1, MTLVertexFormatChar },
		{ VERTEX_FORMAT::CHAR2, MTLVertexFormatChar2 },
		{ VERTEX_FORMAT::CHAR3, MTLVertexFormatChar3 },
		{ VERTEX_FORMAT::CHAR4, MTLVertexFormatChar4 },
		{ VERTEX_FORMAT::SHORT1, MTLVertexFormatShort },
		{ VERTEX_FORMAT::SHORT2, MTLVertexFormatShort2 },
		{ VERTEX_FORMAT::SHORT3, MTLVertexFormatShort3 },
		{ VERTEX_FORMAT::SHORT4, MTLVertexFormatShort4 },
		{ VERTEX_FORMAT::INT1, MTLVertexFormatInt },
		{ VERTEX_FORMAT::INT2, MTLVertexFormatInt2 },
		{ VERTEX_FORMAT::INT3, MTLVertexFormatInt3 },
		{ VERTEX_FORMAT::INT4, MTLVertexFormatInt4 },
		
		{ VERTEX_FORMAT::UCHAR1_NORM, MTLVertexFormatUCharNormalized },
		{ VERTEX_FORMAT::UCHAR2_NORM, MTLVertexFormatUChar2Normalized },
		{ VERTEX_FORMAT::UCHAR3_NORM, MTLVertexFormatUChar3Normalized },
		{ VERTEX_FORMAT::UCHAR4_NORM, MTLVertexFormatUChar4Normalized },
		{ VERTEX_FORMAT::USHORT1_NORM, MTLVertexFormatUShortNormalized },
		{ VERTEX_FORMAT::USHORT2_NORM, MTLVertexFormatUShort2Normalized },
		{ VERTEX_FORMAT::USHORT3_NORM, MTLVertexFormatUShort3Normalized },
		{ VERTEX_FORMAT::USHORT4_NORM, MTLVertexFormatUShort4Normalized },
		{ VERTEX_FORMAT::USHORT4_NORM_BGRA, MTLVertexFormatUChar4Normalized_BGRA },
		
		{ VERTEX_FORMAT::CHAR1_NORM, MTLVertexFormatCharNormalized },
		{ VERTEX_FORMAT::CHAR2_NORM, MTLVertexFormatChar2Normalized },
		{ VERTEX_FORMAT::CHAR3_NORM, MTLVertexFormatChar3Normalized },
		{ VERTEX_FORMAT::CHAR4_NORM, MTLVertexFormatChar4Normalized },
		{ VERTEX_FORMAT::SHORT1_NORM, MTLVertexFormatShortNormalized },
		{ VERTEX_FORMAT::SHORT2_NORM, MTLVertexFormatShort2Normalized },
		{ VERTEX_FORMAT::SHORT3_NORM, MTLVertexFormatShort3Normalized },
		{ VERTEX_FORMAT::SHORT4_NORM, MTLVertexFormatShort4Normalized },
		
		{ VERTEX_FORMAT::U1010102_NORM, MTLVertexFormatUInt1010102Normalized },
		{ VERTEX_FORMAT::I1010102_NORM, MTLVertexFormatInt1010102Normalized },
	};
	const auto metal_vertex_format = format_lut.find(vertex_format);
	if (metal_vertex_format == end(format_lut)) {
		return MTLVertexFormatInvalid;
	}
	return metal_vertex_format->second;
}

MTLIndexType metal_pipeline::metal_index_type_from_index_type(const INDEX_TYPE& index_type) {
	switch (index_type) {
		case INDEX_TYPE::UINT:
			return MTLIndexTypeUInt32;
		case INDEX_TYPE::USHORT:
			return MTLIndexTypeUInt16;
	}
}

const metal_pipeline::metal_pipeline_entry* metal_pipeline::get_metal_pipeline_entry(const device& dev) const {
	if (const auto iter = pipelines.find(&dev); iter != pipelines.end()) {
		return &iter->second;
	}
	return nullptr;
}

} // namespace fl

#endif
