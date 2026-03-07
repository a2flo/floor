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

#include <floor/device/metal/metal4_pipeline.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/core/essentials.hpp>
#include <floor/device/metal/metal4_program.hpp>
#include <floor/device/metal/metal_program.hpp>
#include <floor/device/metal/metal4_function.hpp>
#include <floor/device/metal/metal4_function_entry.hpp>
#include <floor/device/metal/metal_device.hpp>
#include <floor/device/metal/metal_context.hpp>
#include <floor/device/metal/metal4_args.hpp>
#include <floor/floor.hpp>

namespace fl {
using namespace toolchain;

metal4_pipeline::metal4_pipeline(const render_pipeline_description& pipeline_desc_,
								 const std::vector<std::unique_ptr<device>>& devices,
								 const bool with_multi_view_support) :
graphics_pipeline(pipeline_desc_, with_multi_view_support) {
	// NOTE: with Metal, we don't actually have to create an extra pipeline for multi-view support
	
	const auto mtl_vs = (const metal4_function*)pipeline_desc.vertex_shader;
	const auto mtl_fs = (const metal4_function*)pipeline_desc.fragment_shader;
	
	static const bool dump_reflection_info = floor::get_metal_dump_reflection_info();
	
	@autoreleasepool {
		for (const auto& dev : devices) {
			const auto& mtl_dev = ((const metal_device&)*dev).device;
			const auto mtl_vs_entry = (const metal4_function_entry*)mtl_vs->get_function_entry(*dev);
			const auto mtl_fs_entry = (mtl_fs != nullptr ? (const metal4_function_entry*)mtl_fs->get_function_entry(*dev) : nullptr);
			
			MTL4RenderPipelineDescriptor* mtl_pipeline_desc = [MTL4RenderPipelineDescriptor new];
			mtl_pipeline_desc.options = [MTL4PipelineOptions new];
			
			mtl_pipeline_desc.label = (pipeline_desc.debug_label.empty() ? @"metal_graphics_pipeline" :
									   [NSString stringWithUTF8String:(pipeline_desc.debug_label).c_str()]);
			mtl_pipeline_desc.vertexFunctionDescriptor = mtl_vs_entry->function_descriptor;
			mtl_pipeline_desc.fragmentFunctionDescriptor = (mtl_fs_entry != nullptr ? mtl_fs_entry->function_descriptor : nil);
			// NOTE: should always be true, even when no fragment shader is specified as documentation says (e.g. for vs-only shadow rendering)
			mtl_pipeline_desc.rasterizationEnabled = true;
			mtl_pipeline_desc.vertexDescriptor = nil;
			if (!floor::get_metal_soft_indirect()) {
				mtl_pipeline_desc.supportIndirectCommandBuffers = (pipeline_desc.support_indirect_rendering ?
																   MTL4IndirectCommandBufferSupportStateEnabled :
																   MTL4IndirectCommandBufferSupportStateDisabled);
			} else {
				mtl_pipeline_desc.supportIndirectCommandBuffers = MTL4IndirectCommandBufferSupportStateDisabled;
			}
			mtl_pipeline_desc.options.shaderValidation = (MTLShaderValidation)pipeline_desc.validation;
			mtl_pipeline_desc.rasterSampleCount = math::max(pipeline_desc.sample_count, 1u);
			mtl_pipeline_desc.supportVertexBinaryLinking = false;
			mtl_pipeline_desc.supportFragmentBinaryLinking = false;
			mtl_pipeline_desc.colorAttachmentMappingState = MTL4LogicalToPhysicalColorAttachmentMappingStateIdentity;
			
			// set color attachments
			for (size_t i = 0, count = pipeline_desc.color_attachments.size(); i < count; ++i) {
				const auto& color_att = pipeline_desc.color_attachments[i];
				if (color_att.format == IMAGE_TYPE::NONE) {
					log_error("color attachment image type must not be NONE!");
					return;
				}
				
				const auto metal_pixel_format = metal_context::pixel_format_from_image_type(color_att.format);
				if (metal_pixel_format == MTLPixelFormatInvalid) {
					log_error("no matching Metal pixel format found for color image type $X", color_att.format);
					return;
				}
				mtl_pipeline_desc.colorAttachments[i].pixelFormat = metal_pixel_format;
				
				// handle blending
				if (color_att.blend.enable) {
					mtl_pipeline_desc.colorAttachments[i].blendingState = MTL4BlendStateEnabled;
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
					mtl_pipeline_desc.colorAttachments[i].blendingState = MTL4BlendStateDisabled;
				}
			}
			
			// NOTE: it is no longer necessary/possible to set a depth attachment/format ahead of time,
			//       but still check if the specified format is actually supported
			if (pipeline_desc.depth_attachment.format != IMAGE_TYPE::NONE) {
				const auto metal_pixel_format = metal_context::pixel_format_from_image_type(pipeline_desc.depth_attachment.format);
				if (metal_pixel_format == MTLPixelFormatInvalid) {
					log_error("no matching Metal pixel format found for depth image type $X", pipeline_desc.depth_attachment.format);
					return;
				}
			}
			
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
			
			// tessellation is no longer supported
			if (pipeline_desc.tessellation.max_factor > 0u) {
				log_error("tessellation is not supported by Metal 4");
				return;
			}
			if (mtl_vs_entry->info->type != FUNCTION_TYPE::VERTEX) {
				log_error("expected a vertex shader instead of $ ($) in pipeline \"$\"",
						  mtl_vs_entry->info->type, mtl_vs_entry->info->name, pipeline_desc.debug_label);
				return;
			}
			
			// finally create the pipeline object
			metal4_pipeline_entry entry;
			NSError* err = nil;
			id <MTL4Compiler> compiler = metal4_program::get_compiler(*dev);
			if (dump_reflection_info) {
				// need to explicitly request reflections
				mtl_pipeline_desc.options.shaderReflection = (MTL4ShaderReflectionBindingInfo |
															  MTL4ShaderReflectionBufferTypeInfo);
			}
			entry.pipeline_state = [compiler newRenderPipelineStateWithDescriptor:mtl_pipeline_desc
															  compilerTaskOptions:nil
																			error:&err];
			if (!entry.pipeline_state || err) {
				log_error("failed to create pipeline state for device $: $", dev->name,
						  (err != nil ? [[err localizedDescription] UTF8String] : "<no-error-msg>"));
				return;
			}
			
			if (dump_reflection_info && [entry.pipeline_state reflection]) {
				metal_program::dump_bindings_reflection("vertex shader \"" + mtl_vs_entry->info->name + "\"",
														[[entry.pipeline_state reflection] vertexBindings]);
				if (mtl_fs_entry) {
					metal_program::dump_bindings_reflection("fragment shader \"" + mtl_vs_entry->info->name + "\"",
															[[entry.pipeline_state reflection] fragmentBindings]);
				}
			}
			
			entry.vs_entry = mtl_vs_entry;
			entry.fs_entry = mtl_fs_entry;
			entry.pipeline_desc = mtl_pipeline_desc;
			
			// create depth/stencil state
			auto depth_stencil_desc = [MTLDepthStencilDescriptor new];
			depth_stencil_desc.depthWriteEnabled = pipeline_desc.depth.write;
			depth_stencil_desc.depthCompareFunction = metal_compare_func_from_depth_compare(pipeline_desc.depth.compare);
			entry.depth_stencil_state = [mtl_dev newDepthStencilStateWithDescriptor:depth_stencil_desc];
			if (!entry.depth_stencil_state) {
				log_error("failed to create depth/stencil state for device $", dev->name);
				return;
			}
			
#if FLOOR_METAL_DEBUG_RS
			((const metal_device&)*dev).add_debug_allocation(entry.pipeline_state);
#endif
			
			pipelines.emplace(dev.get(), entry);
		}
	}
	
	// success
	valid = true;
}

metal4_pipeline::~metal4_pipeline() {
	@autoreleasepool {
#if FLOOR_METAL_DEBUG_RS
		for (auto&& pipeline : pipelines) {
			((const metal_device*)pipeline.first)->remove_debug_allocation(pipeline.second.pipeline_state);
		}
#endif
		pipelines.clear();
	}
}

MTLPrimitiveType metal4_pipeline::metal_primitive_type_from_primitive(const PRIMITIVE primitive) {
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

MTLCullMode metal4_pipeline::metal_cull_mode_from_cull_mode(const CULL_MODE cull_mode) {
	switch (cull_mode) {
		case CULL_MODE::NONE:
			return MTLCullModeNone;
		case CULL_MODE::BACK:
			return MTLCullModeBack;
		case CULL_MODE::FRONT:
			return MTLCullModeFront;
	}
}

MTLWinding metal4_pipeline::metal_winding_from_front_face(const FRONT_FACE front_face) {
	switch (front_face) {
		case FRONT_FACE::CLOCKWISE:
			return MTLWindingClockwise;
		case FRONT_FACE::COUNTER_CLOCKWISE:
			return MTLWindingCounterClockwise;
	}
}

MTLBlendFactor metal4_pipeline::metal_blend_factor_from_blend_factor(const BLEND_FACTOR blend_factor) {
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

MTLBlendOperation metal4_pipeline::metal_blend_op_from_blend_op(const BLEND_OP blend_op) {
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

MTLCompareFunction metal4_pipeline::metal_compare_func_from_depth_compare(const DEPTH_COMPARE depth_compare) {
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

MTLIndexType metal4_pipeline::metal_index_type_from_index_type(const INDEX_TYPE index_type) {
	switch (index_type) {
		case INDEX_TYPE::UINT:
			return MTLIndexTypeUInt32;
		case INDEX_TYPE::USHORT:
			return MTLIndexTypeUInt16;
	}
}

const metal4_pipeline::metal4_pipeline_entry* metal4_pipeline::get_metal_pipeline_entry(const device& dev) const {
	if (const auto iter = pipelines.find(&dev); iter != pipelines.end()) {
		return &iter->second;
	}
	return nullptr;
}

} // namespace fl

#endif
