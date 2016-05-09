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

#include <floor/compute/cuda/cuda_image.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/floor/floor.hpp>
#include <floor/core/logger.hpp>
#include <floor/compute/cuda/cuda_queue.hpp>
#include <floor/compute/cuda/cuda_device.hpp>
#include <floor/core/gl_shader.hpp>

// internal shaders for copying/blitting opengl textures
static const char blit_vs_text[] {
	"out vec2 tex_coord;\n"
	"void main() {\n"
	"	const vec2 fullscreen_triangle[3] = vec2[](vec2(1.0, 1.0), vec2(-3.0, 1.0), vec2(1.0, -3.0));\n"
	"	tex_coord = fullscreen_triangle[gl_VertexID] * 0.5 + 0.5;\n"
	"	gl_Position = vec4(fullscreen_triangle[gl_VertexID], 0.0, 1.0);\n"
	"}\n"
};
static const char blit_to_color_fs_text[] {
	"uniform sampler2D in_tex;\n"
	"in vec2 tex_coord;\n"
	"out float out_depth;\n"
	"void main() {\n"
	"	out_depth = texture(in_tex, tex_coord).x;\n"
	"}\n"
};
static const char blit_to_depth_fs_text[] {
	"uniform sampler2D in_tex;\n"
	"in vec2 tex_coord;\n"
	"void main() {\n"
	"	gl_FragDepth = texture(in_tex, tex_coord).x;\n"
	"}\n"
};

namespace cuda_image_support {
	enum class CUDA_SHADER : uint32_t {
		BLIT_DEPTH_TO_COLOR,
		BLIT_COLOR_TO_DEPTH,
		__MAX_CUDA_SHADER
	};
	static array<floor_shader_object, (uint32_t)CUDA_SHADER::__MAX_CUDA_SHADER> shaders;
	
	static void init() {
		static bool did_init = false;
		if(did_init) return;
		did_init = true;
		
		// compile internal shaders
		const auto depth_to_color_shd = floor_compile_shader("BLIT_DEPTH_TO_COLOR", blit_vs_text, blit_to_color_fs_text);
		if(!depth_to_color_shd.first) {
			log_error("failed to compile internal shader: %s", depth_to_color_shd.second.name);
		}
		else shaders[(uint32_t)CUDA_SHADER::BLIT_DEPTH_TO_COLOR] = depth_to_color_shd.second;
		
		const auto color_to_depth_shd = floor_compile_shader("BLIT_COLOR_TO_DEPTH", blit_vs_text, blit_to_depth_fs_text);
		if(!color_to_depth_shd.first) {
			log_error("failed to compile internal shader: %s", color_to_depth_shd.second.name);
		}
		else shaders[(uint32_t)CUDA_SHADER::BLIT_COLOR_TO_DEPTH] = color_to_depth_shd.second;
	}
}

template <CU_MEMORY_TYPE src, CU_MEMORY_TYPE dst>
static bool cuda_memcpy(const void* host,
						cu_array device,
						const uint32_t pitch,
						const uint32_t height,
						const uint32_t depth,
						bool async = false,
						cu_stream stream = nullptr) {
	static_assert((src == CU_MEMORY_TYPE::HOST || src == CU_MEMORY_TYPE::ARRAY) &&
				  (dst == CU_MEMORY_TYPE::HOST || dst == CU_MEMORY_TYPE::ARRAY),
				  "invalid src/dst memory type");
	cu_memcpy_3d_descriptor mcpy3d;
	memset(&mcpy3d, 0, sizeof(cu_memcpy_3d_descriptor));
	
	if(src == CU_MEMORY_TYPE::HOST) {
		mcpy3d.src.memory_type = CU_MEMORY_TYPE::HOST;
		mcpy3d.src.host_ptr = host;
		mcpy3d.src.pitch = pitch;
		mcpy3d.src.height = height;
		
		mcpy3d.dst.memory_type = CU_MEMORY_TYPE::ARRAY;
		mcpy3d.dst.array = device;
	}
	else {
		mcpy3d.src.memory_type = CU_MEMORY_TYPE::ARRAY;
		mcpy3d.src.array = device;
		
		mcpy3d.dst.memory_type = CU_MEMORY_TYPE::HOST;
		mcpy3d.dst.host_ptr = host;
		mcpy3d.dst.pitch = pitch;
		mcpy3d.dst.height = height;
	}
	
	mcpy3d.width_in_bytes = pitch;
	mcpy3d.height = height;
	mcpy3d.depth = max(depth, 1u);
	
	if(!async) {
		CU_CALL_RET(cu_memcpy_3d(&mcpy3d), "failed to copy memory", false);
	}
	else {
		CU_CALL_RET(cu_memcpy_3d_async(&mcpy3d, stream), "failed to copy memory", false);
	}
	return true;
}

void cuda_image::init_internal() {
	// only need to (can) init gl shaders when there's a window / gl context
	if(!floor::is_console_only()) {
		cuda_image_support::init();
	}
}

// TODO: proper error (return) value handling everywhere

cuda_image::cuda_image(const cuda_device* device,
					   const uint4 image_dim_,
					   const COMPUTE_IMAGE_TYPE image_type_,
					   void* host_ptr_,
					   const COMPUTE_MEMORY_FLAG flags_,
					   const uint32_t opengl_type_,
					   const uint32_t external_gl_object_,
					   const opengl_image_info* gl_image_info) :
compute_image(device, image_dim_, image_type_, host_ptr_, flags_,
			  opengl_type_, external_gl_object_, gl_image_info) {
	// TODO: handle the remaining flags + host ptr
	
	// zero init cuda textures array
	textures.fill(0);
	
	// need to allocate the buffer on the correct device, if a context was specified,
	// else: assume the correct context is already active
	if(device->ctx != nullptr) {
		CU_CALL_RET(cu_ctx_set_current(device->ctx),
					"failed to make cuda context current");
	}
	
	// actually create the image
	if(!create_internal(true, nullptr)) {
		return; // can't do much else
	}
}

bool cuda_image::create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue) {
	// image handling in cuda/ptx is rather complicated:
	// when using a texture reference (sm_20+) or object (sm_30+), you can only read from it, but with sampler support,
	// when using a surface reference (sm_20+) or object (sm_30+), you can read _and_ write from/to it, but without sampler support.
	// this is further complicated by the fact that I currently can't generate texture/surface reference code, thus no sm_20 support
	// right now + even then, setting and using a texture/surface reference is rather tricky as it is a global module symbol.
	// if "no sampler" flag is set, only use surfaces
	const bool no_sampler { has_flag<COMPUTE_IMAGE_TYPE::FLAG_NO_SAMPLER>(image_type) };
	const bool need_tex { has_flag<COMPUTE_MEMORY_FLAG::READ>(flags) && !no_sampler };
	const bool need_surf { has_flag<COMPUTE_MEMORY_FLAG::WRITE>(flags) || no_sampler };
	
	//
	const auto dim_count = image_dim_count(image_type);
	const auto is_compressed = image_compressed(image_type);
	auto channel_count = image_channel_count(image_type);
	if(channel_count == 3 && !is_compressed) {
		log_error("3-channel images are unsupported with cuda!");
		// TODO: make this work transparently by using an empty alpha channel (pun not intended ;)),
		// this will certainly segfault when using a host pointer that only points to 3-channel data
		// -> on the device: can also make sure it only returns a <type>3 vector
		// NOTE: explicitly fail when trying to use an external opengl image (this would require a copy
		// every time it's used by cuda, which is almost certainly not wanted). also signal this is creating
		// an RGBA image when this is creating the opengl image (warning?).
		//channel_count = 4;
		return false;
	}
	
	fixed_depth = 1;
	uint32_t depth = 0, layer_count = 1;
	if(dim_count >= 3) {
		depth = image_dim.z;
	}
	else {
		// check array first ...
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type)) {
			if(dim_count == 1) depth = image_dim.y;
			else if(dim_count >= 2) depth = image_dim.z;
			layer_count = max(1u, depth);
			fixed_depth = layer_count;
		}
		// ... and check cube map second
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type)) {
			// if FLAG_ARRAY is also present, .z/depth has been specified by the user -> multiply by 6,
			// else, just specify 6 directly
			fixed_depth *= 6;
			depth = (depth != 0 ? depth * 6 : 6);
			
			// make sure width == height
			if(image_dim.x != image_dim.y) {
				log_error("cube map side width and height must be equal (%u != %u)!", image_dim.x, image_dim.y);
				return false;
			}
		}
	}
	
	//
	CU_ARRAY_FORMAT format;
	CU_RESOURCE_VIEW_FORMAT rsrc_view_format;
	
	static const unordered_map<COMPUTE_IMAGE_TYPE, pair<CU_ARRAY_FORMAT, CU_RESOURCE_VIEW_FORMAT>> format_lut {
		{ COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FORMAT_8, { CU_ARRAY_FORMAT::SIGNED_INT8, CU_RESOURCE_VIEW_FORMAT::SINT_1X8 } },
		{ COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FORMAT_16, { CU_ARRAY_FORMAT::SIGNED_INT16, CU_RESOURCE_VIEW_FORMAT::SINT_1X16 } },
		{ COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FORMAT_32, { CU_ARRAY_FORMAT::SIGNED_INT32, CU_RESOURCE_VIEW_FORMAT::SINT_1X32 } },
		{ COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_8, { CU_ARRAY_FORMAT::UNSIGNED_INT8, CU_RESOURCE_VIEW_FORMAT::UINT_1X8 } },
		{ COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_16, { CU_ARRAY_FORMAT::UNSIGNED_INT16, CU_RESOURCE_VIEW_FORMAT::UINT_1X16 } },
		{ COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_24, { CU_ARRAY_FORMAT::UNSIGNED_INT32, CU_RESOURCE_VIEW_FORMAT::UINT_1X32 } },
		{ COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_32, { CU_ARRAY_FORMAT::UNSIGNED_INT32, CU_RESOURCE_VIEW_FORMAT::UINT_1X32 } },
		{ COMPUTE_IMAGE_TYPE::FLOAT | COMPUTE_IMAGE_TYPE::FORMAT_16, { CU_ARRAY_FORMAT::HALF, CU_RESOURCE_VIEW_FORMAT::FLOAT_1X16 } },
		{ COMPUTE_IMAGE_TYPE::FLOAT | COMPUTE_IMAGE_TYPE::FORMAT_32, { CU_ARRAY_FORMAT::FLOAT, CU_RESOURCE_VIEW_FORMAT::FLOAT_1X32 } },
		// all BC formats must be UNSIGNED_INT32, only chanel count differs (BC1-4: 2 channels, BC5-7: 4 channels)
		{
			COMPUTE_IMAGE_TYPE::BC1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_1,
			{ CU_ARRAY_FORMAT::UNSIGNED_INT32, CU_RESOURCE_VIEW_FORMAT::UNSIGNED_BC1 }
		},
		{
			COMPUTE_IMAGE_TYPE::BC2 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_2,
			{ CU_ARRAY_FORMAT::UNSIGNED_INT32, CU_RESOURCE_VIEW_FORMAT::UNSIGNED_BC2 }
		},
		{
			COMPUTE_IMAGE_TYPE::BC3 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_2,
			{ CU_ARRAY_FORMAT::UNSIGNED_INT32, CU_RESOURCE_VIEW_FORMAT::UNSIGNED_BC3 }
		},
		// NOTE: same for BC4/BC5, BC5 fixup based on channel count later
		{
			COMPUTE_IMAGE_TYPE::RGTC | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_4,
			{ CU_ARRAY_FORMAT::UNSIGNED_INT32, CU_RESOURCE_VIEW_FORMAT::UNSIGNED_BC4 }
		},
		{
			COMPUTE_IMAGE_TYPE::RGTC | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FORMAT_4,
			{ CU_ARRAY_FORMAT::UNSIGNED_INT32, CU_RESOURCE_VIEW_FORMAT::SIGNED_BC4 }
		},
		// NOTE: same for signed/unsigned BC6H, unsigned fixup based on normalized flag later
		{
			COMPUTE_IMAGE_TYPE::BPTC | COMPUTE_IMAGE_TYPE::FLOAT | COMPUTE_IMAGE_TYPE::FORMAT_3_3_2,
			{ CU_ARRAY_FORMAT::UNSIGNED_INT32, CU_RESOURCE_VIEW_FORMAT::SIGNED_BC6H }
		},
		{
			COMPUTE_IMAGE_TYPE::BPTC | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_2,
			{ CU_ARRAY_FORMAT::UNSIGNED_INT32, CU_RESOURCE_VIEW_FORMAT::UNSIGNED_BC7 }
		},
	};
	const auto cuda_format = format_lut.find(image_type & (COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK |
														   COMPUTE_IMAGE_TYPE::__COMPRESSION_MASK |
														   COMPUTE_IMAGE_TYPE::__FORMAT_MASK));
	if(cuda_format == end(format_lut)) {
		log_error("unsupported image format: %X", image_type);
		return false;
	}
	
	format = cuda_format->second.first;
	rsrc_view_format = cuda_format->second.second;
	if(!is_compressed) {
		rsrc_view_format = (CU_RESOURCE_VIEW_FORMAT)(uint32_t(rsrc_view_format) + (channel_count == 1 ? 0 :
																				   (channel_count == 2 ? 1 : 2 /* 4 channels */)));
	}
	else {
		// BC5 and BC6H fixup
		if(rsrc_view_format == CU_RESOURCE_VIEW_FORMAT::UNSIGNED_BC4 && channel_count == 2) {
			rsrc_view_format = CU_RESOURCE_VIEW_FORMAT::UNSIGNED_BC5;
		}
		else if(rsrc_view_format == CU_RESOURCE_VIEW_FORMAT::SIGNED_BC4 && channel_count == 2) {
			rsrc_view_format = CU_RESOURCE_VIEW_FORMAT::SIGNED_BC5;
		}
		else if(rsrc_view_format == CU_RESOURCE_VIEW_FORMAT::SIGNED_BC6H && has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type)) {
			rsrc_view_format = CU_RESOURCE_VIEW_FORMAT::UNSIGNED_BC6H;
		}
		
		// fix cuda channel count, cuda documentation says:
		// BC1 - BC4: 2 channels, BC5-7: 4 channels
		switch(rsrc_view_format) {
			case CU_RESOURCE_VIEW_FORMAT::UNSIGNED_BC1:
			case CU_RESOURCE_VIEW_FORMAT::UNSIGNED_BC2:
			case CU_RESOURCE_VIEW_FORMAT::UNSIGNED_BC3:
			case CU_RESOURCE_VIEW_FORMAT::UNSIGNED_BC4:
			case CU_RESOURCE_VIEW_FORMAT::SIGNED_BC4:
				channel_count = 2;
				break;
			case CU_RESOURCE_VIEW_FORMAT::UNSIGNED_BC5:
			case CU_RESOURCE_VIEW_FORMAT::SIGNED_BC5:
			case CU_RESOURCE_VIEW_FORMAT::UNSIGNED_BC6H:
			case CU_RESOURCE_VIEW_FORMAT::SIGNED_BC6H:
			case CU_RESOURCE_VIEW_FORMAT::UNSIGNED_BC7:
				channel_count = 4;
				break;
			default: floor_unreachable();
		}
	}
	
	// -> cuda array
	if(!has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags)) {
		const cu_array_3d_descriptor desc {
			.dim = {
				image_dim.x,
				(dim_count >= 2 ? image_dim.y : 0),
				depth
			},
			.format = format,
			.channel_count = channel_count,
			.flags = (
					  (has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) ? CU_ARRAY_3D_FLAGS::LAYERED : CU_ARRAY_3D_FLAGS::NONE) |
					  (has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type) ? CU_ARRAY_3D_FLAGS::CUBE_MAP : CU_ARRAY_3D_FLAGS::NONE) |
					  // NOTE: depth flag is not supported and array creation will return INVALID_VALUE
					  // (has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type) ? CU_ARRAY_3D_FLAGS::DEPTH_TEXTURE : CU_ARRAY_3D_FLAGS::NONE) |
					  (has_flag<COMPUTE_IMAGE_TYPE::FLAG_GATHER>(image_type) ? CU_ARRAY_3D_FLAGS::TEXTURE_GATHER : CU_ARRAY_3D_FLAGS::NONE) |
					  (need_surf ? CU_ARRAY_3D_FLAGS::SURFACE_LOAD_STORE : CU_ARRAY_3D_FLAGS::NONE)
			)
		};
		log_debug("surf/tex %u/%u; dim %u: %v; channels %u; flags %u; format: %X",
				  need_surf, need_tex, dim_count, desc.dim, desc.channel_count, desc.flags, desc.format);
		if(!is_mip_mapped) {
			CU_CALL_RET(cu_array_3d_create(&image_array, &desc),
						"failed to create cuda array/image", false);
			image = image_array;
		}
		else {
			CU_CALL_RET(cu_mipmapped_array_create(&image_mipmap_array, &desc, mip_level_count),
						"failed to create cuda mip-mapped array/image", false);
			image = image_mipmap_array;
			
			image_mipmap_arrays.resize(mip_level_count);
			for(uint32_t level = 0; level < mip_level_count; ++level) {
				CU_CALL_RET(cu_mipmapped_array_get_level(&image_mipmap_arrays[level], image_mipmap_array, level),
							"failed to retrieve cuda mip-map level #" + to_string(level), false);
			}
		}
		
		// copy host memory to device if it is non-null and NO_INITIAL_COPY is not specified
		if(copy_host_data && host_ptr != nullptr && !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
			log_debug("copying %u bytes from %X to array %X",
					  image_data_size, host_ptr, image);
			auto cpy_host_ptr = (uint8_t*)host_ptr;
			apply_on_levels([this, &cpy_host_ptr](const uint32_t& level,
												  const uint4& mip_image_dim,
												  const uint32_t& slice_data_size,
												  const uint32_t& level_data_size) {
				if(!cuda_memcpy<CU_MEMORY_TYPE::HOST, CU_MEMORY_TYPE::ARRAY>(cpy_host_ptr,
																			 (is_mip_mapped ? image_mipmap_arrays[level] : image_array),
																			 slice_data_size / max(mip_image_dim.y, 1u),
																			 mip_image_dim.y, mip_image_dim.z * fixed_depth)) {
					log_error("failed to copy initial host data to device");
					return false;
				}
				cpy_host_ptr += level_data_size;
				return true;
			});
		}
	}
	// -> opengl image
	else {
		if(!create_gl_image(copy_host_data)) return false;
		log_debug("surf/tex %u/%u", need_surf, need_tex);
		
		// cuda doesn't support depth textures
		// -> need to create a compatible texture and copy it on the gpu
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type)) {
			// remove old
			if(depth_compat_tex != 0) {
				glDeleteTextures(1, &depth_compat_tex);
			}
			
			// check if the format can be used
			switch(gl_internal_format) {
				case GL_DEPTH_COMPONENT16:
					depth_compat_format = GL_R16UI;
					break;
				case GL_DEPTH_COMPONENT24:
				case GL_DEPTH_COMPONENT32:
					depth_compat_format = GL_R32UI;
					break;
				case GL_DEPTH_COMPONENT32F:
					depth_compat_format = GL_R32F;
					break;
				case GL_DEPTH32F_STENCIL8:
					depth_compat_format = GL_R32F;
					
					// correct view format, since stencil isn't supported
					rsrc_view_format = CU_RESOURCE_VIEW_FORMAT::FLOAT_1X32;
					break;
				default:
					log_error("can't share opengl depth format %X with cuda", gl_internal_format);
					return false;
			}
			
			//
			glGenTextures(1, &depth_compat_tex);
			glBindTexture(opengl_type, depth_compat_tex);
			glTexParameteri(opengl_type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(opengl_type, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(opengl_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(opengl_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			if(dim_count == 2) {
				glTexImage2D(opengl_type, 0, (GLint)depth_compat_format, (int)image_dim.x, (int)image_dim.y,
							 0, GL_RED, gl_type, nullptr);
			}
			else {
				glTexParameteri(opengl_type, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				glTexImage3D(opengl_type, 0, (GLint)depth_compat_format, (int)image_dim.x, (int)image_dim.y, (int)image_dim.z,
							 0, GL_RED, gl_type, nullptr);
			}
			
			// need a copy fbo when ARB_copy_image is not available
			if(!floor::has_opengl_extension("GL_ARB_copy_image")) {
				// check if depth 2D image, others are not supported (stencil should work by simply being dropped)
				if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) ||
				   has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type) ||
				   has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type)) {
					log_error("unsupported depth image format (%X), only 2D depth or depth+stencil is supported!",
							  image_type);
					return false;
				}
				
				// cleanup
				if(depth_copy_fbo != 0) {
					glDeleteFramebuffers(1, &depth_copy_fbo);
				}
				
				glGenFramebuffers(1, &depth_copy_fbo);
				glBindFramebuffer(GL_FRAMEBUFFER, depth_copy_fbo);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, opengl_type, depth_compat_tex, 0);
				
				// check for gl/fbo errors
				const auto err = glGetError();
				const auto fbo_err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
				if(err != 0 || fbo_err != GL_FRAMEBUFFER_COMPLETE) {
					log_error("depth compat fbo/tex error: %X %X", err, fbo_err);
					return false;
				}
				
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			}
			glBindTexture(opengl_type, 0);
		}
		
		// register the cuda object
		CU_GRAPHICS_REGISTER_FLAGS cuda_gl_flags;
		switch(flags & COMPUTE_MEMORY_FLAG::READ_WRITE) {
			case COMPUTE_MEMORY_FLAG::READ:
				cuda_gl_flags = CU_GRAPHICS_REGISTER_FLAGS::READ_ONLY;
				break;
			case COMPUTE_MEMORY_FLAG::WRITE:
				cuda_gl_flags = CU_GRAPHICS_REGISTER_FLAGS::WRITE_DISCARD;
				break;
			default:
			case COMPUTE_MEMORY_FLAG::READ_WRITE:
				cuda_gl_flags = CU_GRAPHICS_REGISTER_FLAGS::NONE;
				break;
		}
		CU_CALL_RET(cu_graphics_gl_register_image(&rsrc,
												  (depth_compat_tex == 0 ? gl_object : depth_compat_tex),
												  opengl_type, cuda_gl_flags),
					"failed to register opengl image with cuda", false);
		if(rsrc == nullptr) {
			log_error("created cuda gl graphics resource is invalid!");
			return false;
		}
		// acquire for use with cuda
		acquire_opengl_object(cqueue);
	}
	
	// create texture/surface objects, depending on read/write flags and sampler support (TODO: and sm_xy)
	cu_resource_descriptor rsrc_desc;
	cu_resource_view_descriptor rsrc_view_desc;
	memset(&rsrc_desc, 0, sizeof(cu_resource_descriptor));
	memset(&rsrc_view_desc, 0, sizeof(cu_resource_view_descriptor));
	
	// TODO: support LINEAR/PITCH2D?
	if(is_mip_mapped) {
		rsrc_desc.type = CU_RESOURCE_TYPE::MIP_MAPPED_ARRAY;
		rsrc_desc.mip_mapped_array = image_mipmap_array;
	}
	else {
		rsrc_desc.type = CU_RESOURCE_TYPE::ARRAY;
		rsrc_desc.array = image_array;
	}
	
	if(need_tex) {
		rsrc_view_desc.format = rsrc_view_format;
		rsrc_view_desc.dim = {
			image_dim.x,
			(dim_count >= 2 ? image_dim.y : 0),
			depth
		};
		rsrc_view_desc.first_mip_map_level = 0;
		rsrc_view_desc.last_mip_map_level = mip_level_count - 1;
		rsrc_view_desc.first_layer = 0;
		rsrc_view_desc.last_layer = layer_count - 1;
		
		for(uint32_t i = 0, count = uint32_t(size(textures)); i < count; ++i) {
			cu_texture_descriptor tex_desc;
			memset(&tex_desc, 0, sizeof(cu_texture_descriptor));
			
			// address mode is fixed for now
			tex_desc.address_mode[0] = CU_ADDRESS_MODE::CLAMP;
			if(dim_count >= 2) tex_desc.address_mode[1] = CU_ADDRESS_MODE::CLAMP;
			if(dim_count >= 3) tex_desc.address_mode[2] = CU_ADDRESS_MODE::CLAMP;
			
			// filter mode
			const auto filter_mode = (cuda_sampler::get_filter_mode(i) == cuda_sampler::NEAREST ?
									  CU_FILTER_MODE::NEAREST : CU_FILTER_MODE::LINEAR);
			tex_desc.filter_mode = filter_mode;
			tex_desc.mip_map_filter_mode = filter_mode;
			
			// non-normalized / normalized coordinates
			const auto coord_mode = (cuda_sampler::get_coord_mode(i) == cuda_sampler::PIXEL ?
									 CU_TEXTURE_FLAGS::NONE : CU_TEXTURE_FLAGS::NORMALIZED_COORDINATES);
			tex_desc.flags = coord_mode;
			
			// no variable anisotropy yet
			tex_desc.max_anisotropy = 16;
			tex_desc.min_mip_map_level_clamp = 0;
			tex_desc.max_mip_map_level_clamp = (is_mip_mapped ? dev->max_mip_levels : 0);
			
			cu_tex_object new_texture = 0;
			CU_CALL_RET(cu_tex_object_create(&new_texture, &rsrc_desc, &tex_desc, &rsrc_view_desc),
						"failed to create texture object #" + to_string(i), false);
			// we can do this, because cuda only tracks/returns the lower 32-bit of cu_tex_object
			textures[i] = (cu_tex_only_object)new_texture;
			
#if defined(FLOOR_CUDA_USE_INTERNAL_API) // TODO/NOTE: wip h/w depth compare mode setting
			const auto compare_function = cuda_sampler::get_compare_function(i);
			if(compare_function != cuda_sampler::NONE) {
				cu_texture_ref tex_ref = nullptr;
				cuda_api.get_tex_ref_from_tex_obj(((cuda_device*)dev)->ctx->sampler_pool, textures[i], &tex_ref);
				
				switch(compare_function) {
					case cuda_sampler::LESS:
						tex_ref->sampler_enum.compare_function = CU_SAMPLER_TYPE::COMPARE_FUNCTION::LESS;
						break;
					case cuda_sampler::LESS_OR_EQUAL:
						tex_ref->sampler_enum.compare_function = CU_SAMPLER_TYPE::COMPARE_FUNCTION::LESS_OR_EQUAL;
						break;
					case cuda_sampler::GREATER:
						tex_ref->sampler_enum.compare_function = CU_SAMPLER_TYPE::COMPARE_FUNCTION::GREATER;
						break;
					case cuda_sampler::GREATER_OR_EQUAL:
						tex_ref->sampler_enum.compare_function = CU_SAMPLER_TYPE::COMPARE_FUNCTION::GREATER_OR_EQUAL;
						break;
					case cuda_sampler::EQUAL:
						tex_ref->sampler_enum.compare_function = CU_SAMPLER_TYPE::COMPARE_FUNCTION::EQUAL;
						break;
					case cuda_sampler::NOT_EQUAL:
						tex_ref->sampler_enum.compare_function = CU_SAMPLER_TYPE::COMPARE_FUNCTION::NOT_EQUAL;
						break;
					default:
						floor_unreachable();
				}
				
				cuda_api.update_tex_sampler(((cuda_device*)dev)->ctx->sampler_pool,
											textures[i], &tex_ref->sampler_ptr, &tex_ref->sampler_enum);
				
				// TODO: future solution: override/hijack the device function pointer that creates/inits the sampler data,
				// then we don't have to update the sampler data afterwards (a bit more tricky though)
			}
#endif
		}
	}
	if(need_surf) {
		// TODO: support image write with lod (need to create a surface for each mip-level ...)
		if(is_mip_mapped) {
			rsrc_desc.type = CU_RESOURCE_TYPE::ARRAY;
			rsrc_desc.array = image_mipmap_arrays[0];
		}
		CU_CALL_RET(cu_surf_object_create(&surface, &rsrc_desc),
					"failed to create surface object", false);
	}
	
	return true;
}

cuda_image::~cuda_image() {
	// kill the image
	if(image == nullptr) return;
	
	for(const auto& texture : textures) {
		if(texture != 0) {
			CU_CALL_NO_ACTION(cu_tex_object_destroy(texture),
							  "failed to destroy texture object");
		}
	}
	if(surface != 0) {
		CU_CALL_NO_ACTION(cu_surf_object_destroy(surface),
						  "failed to destroy surface object");
	}
	
	// -> cuda array
	if(!has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags)) {
		if(image_array != nullptr) {
			CU_CALL_RET(cu_array_destroy(image_array),
						"failed to free device memory");
		}
		if(image_mipmap_array != nullptr) {
			CU_CALL_RET(cu_mipmapped_array_destroy(image_mipmap_array),
						"failed to free device memory");
		}
	}
	// -> opengl image
	else {
		if(gl_object == 0) {
			log_error("invalid opengl image!");
		}
		else {
			if(image == nullptr || gl_object_state) {
				log_warn("image still registered for opengl use - acquire before destructing a compute image!");
			}
			// kill opengl image
			if(!gl_object_state) release_opengl_object(nullptr); // -> release to opengl
			delete_gl_image();
		}
	}
	
	// clean up depth compat objects
	if(depth_copy_fbo != 0) {
		glDeleteFramebuffers(1, &depth_copy_fbo);
	}
	if(depth_compat_tex != 0) {
		glDeleteTextures(1, &depth_compat_tex);
	}
}

void cuda_image::zero(shared_ptr<compute_queue> cqueue) {
	if(image == nullptr) return;
	
	// NOTE: when using mip-mapping, we can reuse the zero data ptr from the first level (all levels will be smaller than the first)
	const auto first_level_size = image_data_size_from_types(image_dim, image_type, 1, true);
	auto zero_data = make_unique<uint8_t[]>(first_level_size);
	auto zero_data_ptr = zero_data.get();
	memset(zero_data_ptr, 0, first_level_size);
	
	apply_on_levels([this, &zero_data_ptr](const uint32_t& level,
										   const uint4& mip_image_dim,
										   const uint32_t& slice_data_size,
										   const uint32_t&) {
		if(!cuda_memcpy<CU_MEMORY_TYPE::HOST, CU_MEMORY_TYPE::ARRAY>(zero_data_ptr,
																	 (is_mip_mapped ? image_mipmap_arrays[level] : image_array),
																	 slice_data_size / max(mip_image_dim.y, 1u),
																	 mip_image_dim.y, mip_image_dim.z * fixed_depth)) {
			log_error("failed to zero image");
			return false;
		}
		return true;
	});
	
	cqueue->finish();
}

void* __attribute__((aligned(128))) cuda_image::map(shared_ptr<compute_queue> cqueue, const COMPUTE_MEMORY_MAP_FLAG flags_) {
	if(image == nullptr) return nullptr;
	
	// TODO: parameter origin + region
	// NOTE: a) not supported with mip-mapping if region != image size, b) must update all map/unmap code (relies on region == image size right now)
	const size_t dim_count = image_dim_count(image_type);
	uint32_t depth = 0;
	if(dim_count >= 3) {
		depth = image_dim.z;
	}
	else {
		// check array first ...
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type)) {
			if(dim_count == 1) depth = image_dim.y;
			else if(dim_count >= 2) depth = image_dim.z;
		}
		// ... and check cube map second
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type)) {
			depth = (depth != 0 ? depth * 6 : 6);
		}
	}
	const size_t height = (dim_count >= 2 ? image_dim.y : 1);

	const size3 origin { 0, 0, 0 };
	const size3 region { size3(image_dim.x, height, depth).maxed(1) }; // complete image(s) + "The values in region cannot be 0."
	const auto map_size = region.x * region.y * region.z * image_bytes_per_pixel(image_type);
	
	const bool blocking_map = has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(flags_);
	// TODO: image map check
	
	bool write_only = false;
	if(has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(flags_)) {
		write_only = true;
	}
	else {
		switch(flags_ & COMPUTE_MEMORY_MAP_FLAG::READ_WRITE) {
			case COMPUTE_MEMORY_MAP_FLAG::READ:
				write_only = false;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::WRITE:
				write_only = true;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::READ_WRITE:
				write_only = false;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::NONE:
			default:
				log_error("neither read nor write flag set for image mapping!");
				return nullptr;
		}
	}
	
	// alloc host memory (NOTE: not going to use pinned memory here, b/c it has restrictions)
	alignas(128) uint8_t* host_buffer = new uint8_t[map_size] alignas(128);
	
	// check if we need to copy the image from the device (in case READ was specified)
	if(!write_only) {
		if(blocking_map) {
			// must finish up all current work before we can properly read from the current buffer
			cqueue->finish();
		}
		
		auto cpy_host_ptr = host_buffer;
		apply_on_levels([this, &cpy_host_ptr, &blocking_map,
						 stream = (cu_stream)cqueue->get_queue_ptr()](const uint32_t& level,
																	  const uint4& mip_image_dim,
																	  const uint32_t& slice_data_size,
																	  const uint32_t& level_data_size) {
			if(!cuda_memcpy<CU_MEMORY_TYPE::ARRAY, CU_MEMORY_TYPE::HOST>(cpy_host_ptr,
																		 (is_mip_mapped ? image_mipmap_arrays[level] : image_array),
																		 slice_data_size / max(mip_image_dim.y, 1u),
																		 mip_image_dim.y, mip_image_dim.z * fixed_depth,
																		 !blocking_map, stream)) {
				log_error("failed to copy device memory to host");
				return false;
			}
			cpy_host_ptr += level_data_size;
			return true;
		});
	}
	
	// need to remember how much we mapped and where (so the host->device copy copies the right amount of bytes)
	mappings.emplace(host_buffer, cuda_mapping { origin, region, flags_ });
	
	return host_buffer;
}

void cuda_image::unmap(shared_ptr<compute_queue> cqueue floor_unused,
					   void* __attribute__((aligned(128))) mapped_ptr) {
	if(image == nullptr) return;
	if(mapped_ptr == nullptr) return;
	
	// check if this is actually a mapped pointer (+get the mapped size)
	const auto iter = mappings.find(mapped_ptr);
	if(iter == mappings.end()) {
		log_error("invalid mapped pointer: %X", mapped_ptr);
		return;
	}
	
	// check if we need to actually copy data back to the device (not the case if read-only mapping)
	if(has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE>(iter->second.flags) ||
	   has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(iter->second.flags)) {
		auto cpy_host_ptr = (uint8_t*)mapped_ptr;
		apply_on_levels([this, &cpy_host_ptr](const uint32_t& level,
											  const uint4& mip_image_dim,
											  const uint32_t& slice_data_size,
											  const uint32_t& level_data_size) {
			if(!cuda_memcpy<CU_MEMORY_TYPE::HOST, CU_MEMORY_TYPE::ARRAY>(cpy_host_ptr,
																		 (is_mip_mapped ? image_mipmap_arrays[level] : image_array),
																		 slice_data_size / max(mip_image_dim.y, 1u),
																		 mip_image_dim.y, mip_image_dim.z * fixed_depth)) {
				log_error("failed to copy host memory to device");
				return false;
			}
			cpy_host_ptr += level_data_size;
			return true;
		});
	}
	
	// free host memory again and remove the mapping
	delete [] (unsigned char*)mapped_ptr;
	mappings.erase(mapped_ptr);
}

template <bool depth_to_color /* or color_to_depth if false */>
floor_inline_always static void copy_depth_texture(const uint32_t& depth_copy_fbo,
												   const uint32_t& input_tex,
												   const uint32_t& output_tex,
												   const uint32_t& opengl_type,
												   const uint4& image_dim) {
	// get current state
	GLint cur_fbo = 0, front_face = 0, cull_face_mode = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &cur_fbo);
	glGetIntegerv(GL_FRONT_FACE, &front_face);
	glGetIntegerv(GL_CULL_FACE_MODE, &cull_face_mode);
	
	// bind our copy fbo and draw / copy the image using a shader
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, depth_copy_fbo);
	if(depth_to_color) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, opengl_type, output_tex, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, opengl_type, 0, 0);
	}
	else {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, opengl_type, 0, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, opengl_type, output_tex, 0);
	}
	glViewport(0, 0, int(image_dim.x), int(image_dim.y));
	
	// cull the right side (geometry in shader is CCW)
	glEnable(GL_CULL_FACE);
	glCullFace(front_face == GL_CCW ? GL_BACK : GL_FRONT);
	
	glUseProgram(cuda_image_support::shaders[(uint32_t)(depth_to_color ?
														cuda_image_support::CUDA_SHADER::BLIT_DEPTH_TO_COLOR :
														cuda_image_support::CUDA_SHADER::BLIT_COLOR_TO_DEPTH)].program.program);
	glUniform1i(0, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(opengl_type, input_tex);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glUseProgram(0);
	
	// restore (note: not goind to store/restore shader state, this is assumed to be unsafe)
	glCullFace((GLuint)cull_face_mode);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)cur_fbo);
}

bool cuda_image::acquire_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(rsrc == nullptr) return false;
	if(!gl_object_state) {
#if defined(FLOOR_DEBUG) && 0
		log_warn("opengl image has already been acquired for use with cuda!");
#endif
		return true;
	}
	
	// if a depth compat texture is used, the original opengl texture must by copied into it
	if(depth_compat_tex != 0 && has_flag<COMPUTE_MEMORY_FLAG::READ>(flags)) {
		if(floor::has_opengl_extension("GL_ARB_copy_image")) {
			glCopyImageSubData(gl_object, opengl_type, 0, 0, 0, 0,
							   depth_compat_tex, opengl_type, 0, 0, 0, 0,
							   (int)image_dim.x, (int)image_dim.y, std::max((int)image_dim.z, 1));
		}
		else {
			copy_depth_texture<true /* depth to color */>(depth_copy_fbo, gl_object, depth_compat_tex, opengl_type, image_dim);
		}
	}
	
	CU_CALL_RET(cu_graphics_map_resources(1, &rsrc,
										  (cqueue != nullptr ? (cu_stream)cqueue->get_queue_ptr() : nullptr)),
				"failed to acquire opengl image - cuda resource mapping failed!", false);
	gl_object_state = false;
	
	// TODO: handle opengl array/layers
	if(is_mip_mapped) {
		CU_CALL_RET(cu_graphics_resource_get_mapped_mipmapped_array(&image_mipmap_array, rsrc),
					"failed to retrieve mapped cuda mip-map image from opengl image!", false);
		image = image_mipmap_array;
		
		image_mipmap_arrays.resize(mip_level_count);
		for(uint32_t level = 0; level < mip_level_count; ++level) {
			CU_CALL_RET(cu_graphics_sub_resource_get_mapped_array(&image_mipmap_arrays[level], rsrc, 0, level),
						"failed to retrieve mip-map level #" + to_string(level) + " from mapped opengl image!", false);
		}
	}
	else {
		CU_CALL_RET(cu_graphics_sub_resource_get_mapped_array(&image_array, rsrc, 0, 0),
					"failed to retrieve mapped cuda image from opengl image!", false);
		image = image_array;
	}
	
	if(image == nullptr) {
		log_error("mapped cuda image (from a graphics resource) is invalid!");
		return false;
	}
	
	return true;
}

bool cuda_image::release_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(image == nullptr) return false;
	if(rsrc == nullptr) return false;
	if(gl_object_state) {
#if defined(FLOOR_DEBUG) && 0
		log_warn("opengl image has already been released for opengl use!");
#endif
		return true;
	}
	
	// if a depth compat texture is used, the cuda image must be copied to the opengl depth texture
	if(depth_compat_tex != 0 && has_flag<COMPUTE_MEMORY_FLAG::WRITE>(flags)) {
		if(floor::has_opengl_extension("GL_ARB_copy_image")) {
			glCopyImageSubData(depth_compat_tex, opengl_type, 0, 0, 0, 0,
							   gl_object, opengl_type, 0, 0, 0, 0,
							   (int)image_dim.x, (int)image_dim.y, std::max((int)image_dim.z, 1));
		}
		else {
			copy_depth_texture<false /* color to depth */>(depth_copy_fbo, depth_compat_tex, gl_object, opengl_type, image_dim);
		}
	}
	
	// reset array pointers (these are no longer valid) + unmap resource
	image = nullptr;
	image_array = nullptr;
	image_mipmap_array = nullptr;
	image_mipmap_arrays.clear();
	CU_CALL_RET(cu_graphics_unmap_resources(1, &rsrc,
											(cqueue != nullptr ? (cu_stream)cqueue->get_queue_ptr() : nullptr)),
				"failed to release opengl image - cuda resource unmapping failed!", false);
	gl_object_state = true;
	
	return true;
}

#endif
