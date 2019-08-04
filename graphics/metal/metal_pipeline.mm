/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#include <floor/graphics/metal/metal_pipeline.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/core/essentials.hpp>
#include <floor/compute/metal/metal_program.hpp>
#include <floor/compute/metal/metal_kernel.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/metal/metal_image.hpp>

metal_pipeline::metal_pipeline(const render_pipeline_description& pipeline_desc_, const vector<unique_ptr<compute_device>>& devices) :
graphics_pipeline(pipeline_desc_) {
	const auto mtl_vs = (const metal_kernel*)pipeline_desc.vertex_shader;
	const auto mtl_fs = (const metal_kernel*)pipeline_desc.fragment_shader;
	
	for (const auto& dev : devices) {
		const auto& mtl_dev = ((const metal_device&)*dev).device;
		const auto mtl_vs_entry = (const metal_kernel::metal_kernel_entry*)mtl_vs->get_kernel_entry(*dev);
		const auto mtl_fs_entry = (mtl_fs != nullptr ? (const metal_kernel::metal_kernel_entry*)mtl_fs->get_kernel_entry(*dev) : nullptr);
	
		MTLRenderPipelineDescriptor* mtl_pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
		mtl_pipeline_desc.label = @"metal pipeline";
		mtl_pipeline_desc.vertexFunction = (__bridge id<MTLFunction>)mtl_vs_entry->kernel;
		mtl_pipeline_desc.fragmentFunction = (mtl_fs_entry != nullptr ? (__bridge id<MTLFunction>)mtl_fs_entry->kernel : nil);
		
		// set color attachments
		for (size_t i = 0, count = pipeline_desc.color_attachments.size(); i < count; ++i) {
			const auto& color_att = pipeline_desc.color_attachments[i];
			if (color_att.format == COMPUTE_IMAGE_TYPE::NONE) {
				log_error("color attachment image type must not be NONE!");
				return;
			}
			
			const auto metal_pixel_format = metal_image::metal_pixel_format_from_image_type(color_att.format);
			if (!metal_pixel_format) {
				log_error("no matching Metal pixel format found for color image type %X", color_att.format);
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
		if (pipeline_desc.depth_attachment.format != COMPUTE_IMAGE_TYPE::NONE) {
			const auto metal_pixel_format = metal_image::metal_pixel_format_from_image_type(pipeline_desc.depth_attachment.format);
			if (!metal_pixel_format) {
				log_error("no matching Metal pixel format found for depth image type %X", pipeline_desc.depth_attachment.format);
				return;
			}
			mtl_pipeline_desc.depthAttachmentPixelFormat = *metal_pixel_format;
		}
		
		// stencil is not supported yet
		mtl_pipeline_desc.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
		
		// set primitive type
		if ([mtl_pipeline_desc respondsToSelector:@selector(inputPrimitiveTopology)]) { // macOS 10.11+ or iOS 12.0+
			MTLPrimitiveTopologyClass primitive = MTLPrimitiveTopologyClass::MTLPrimitiveTopologyClassUnspecified;
			switch (pipeline_desc.primitive) {
				case PRIMITIVE::POINT:
					primitive = MTLPrimitiveTopologyClass::MTLPrimitiveTopologyClassPoint;
					break;
				case PRIMITIVE::LINE:
					primitive = MTLPrimitiveTopologyClass::MTLPrimitiveTopologyClassLine;
					break;
				case PRIMITIVE::TRIANGLE:
				case PRIMITIVE::TRIANGLE_STRIP:
					primitive = MTLPrimitiveTopologyClass::MTLPrimitiveTopologyClassTriangle;
					break;
			}
			mtl_pipeline_desc.inputPrimitiveTopology = primitive;
		}
		
		// TODO: set per-buffer mutability
		//mtl_pipeline_desc.vertexBuffers[0].mutability = MTLMutability::MTLMutabilityMutable;
		//mtl_pipeline_desc.fragmentBuffers[0].mutability = MTLMutability::MTLMutabilityImmutable;
		
		// finally create the pipeline object
		metal_pipeline_entry entry;
		NSError* error = nullptr;
		entry.pipeline_state = [mtl_dev newRenderPipelineStateWithDescriptor:mtl_pipeline_desc error:&error];
		if (!entry.pipeline_state) {
			log_error("failed to create pipeline state for device %s: %s", dev->name,
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
			log_error("failed to create depth/stencil state for device %s", dev->name);
			return;
		}
		
		pipelines.insert(*dev, entry);
	}
	
	// success
	valid = true;
}

metal_pipeline::~metal_pipeline() {
}

MTLPrimitiveType metal_pipeline::metal_primitive_type_from_primitive(const PRIMITIVE& primitive) {
	switch (primitive) {
		case PRIMITIVE::POINT:
			return MTLPrimitiveTypePoint;
		case PRIMITIVE::LINE:
			return MTLPrimitiveTypeLine;
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

const metal_pipeline::metal_pipeline_entry* metal_pipeline::get_metal_pipeline_entry(const compute_device& dev) const {
	const auto ret = pipelines.get(dev);
	return !ret.first ? nullptr : &ret.second->second;
}

#endif
