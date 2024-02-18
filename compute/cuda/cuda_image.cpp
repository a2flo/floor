/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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
#include <floor/compute/cuda/cuda_compute.hpp>
#include <floor/compute/cuda/cuda_buffer.hpp>
#include <floor/core/gl_shader.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/floor/floor.hpp>
#include <floor/compute/vulkan/vulkan_image.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/compute/vulkan/vulkan_semaphore.hpp>
#endif

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
		if (floor::get_renderer() != floor::RENDERER::OPENGL) {
			// don't do anything when we've not actually created an opengl context
			return;
		}
		
		// compile internal shaders
		const auto depth_to_color_shd = floor_compile_shader("BLIT_DEPTH_TO_COLOR", blit_vs_text, blit_to_color_fs_text);
		if(!depth_to_color_shd.first) {
			log_error("failed to compile internal shader: $", depth_to_color_shd.second.name);
		}
		else shaders[(uint32_t)CUDA_SHADER::BLIT_DEPTH_TO_COLOR] = depth_to_color_shd.second;
		
		const auto color_to_depth_shd = floor_compile_shader("BLIT_COLOR_TO_DEPTH", blit_vs_text, blit_to_depth_fs_text);
		if(!color_to_depth_shd.first) {
			log_error("failed to compile internal shader: $", color_to_depth_shd.second.name);
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
						const_cu_stream stream = nullptr) {
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
		CU_CALL_RET(cu_memcpy_3d(&mcpy3d), "failed to copy memory", false)
	}
	else {
		CU_CALL_RET(cu_memcpy_3d_async(&mcpy3d, stream), "failed to copy memory", false)
	}
	return true;
}

static uint32_t cuda_driver_version { 9000 };
void cuda_image::init_internal(cuda_compute* ctx) {
	// only need to (can) init gl shaders when there's a window / gl context
	if(!floor::is_console_only()) {
		cuda_image_support::init();
	}
	
	// need to know the driver version when using internal cuda functionality later on
	cuda_driver_version = ctx->get_cuda_driver_version();
}

static safe_mutex device_sampler_mtx;
static const cuda_device* cur_device { nullptr };
static bool apply_sampler_modifications { false };
static CU_SAMPLER_TYPE cuda_sampler_or { .low = 0, .high = 0 };
CU_RESULT cuda_image::internal_device_sampler_init(cu_texture_ref tex_ref) {
	// TODO: rather use tex_ref->ctx to figure this out (need to figure out how stable this is first though)
	if(cur_device == nullptr) {
		log_error("current cuda device not set!");
		return CU_RESULT::INVALID_VALUE;
	}
	
	// call the original sampler init function
	const auto ret = cur_device->sampler_init_func_ptr(tex_ref);
	
	// only modify the sampler enum if this is wanted (i.e. this will be false when not setting depth compare state)
	if(apply_sampler_modifications) {
		// NOTE: need cuda version dependent offset for this (differs by 16 bytes for cuda 7.5/8.0)
		((cu_texture_ref_80*)tex_ref)->sampler_enum.low |= cuda_sampler_or.low;
		((cu_texture_ref_80*)tex_ref)->sampler_enum.high |= cuda_sampler_or.high;
	}
	
	return ret;
}

// TODO: proper error (return) value handling everywhere

cuda_image::cuda_image(const compute_queue& cqueue,
					   const uint4 image_dim_,
					   const COMPUTE_IMAGE_TYPE image_type_,
					   std::span<uint8_t> host_data_,
					   const COMPUTE_MEMORY_FLAG flags_,
					   const uint32_t opengl_type_,
					   const uint32_t external_gl_object_,
					   const opengl_image_info* gl_image_info,
					   compute_image* shared_image_) :
compute_image(cqueue, image_dim_, image_type_, host_data_, flags_,
			  opengl_type_, external_gl_object_, gl_image_info, shared_image_, false),
is_mip_mapped_or_vulkan(is_mip_mapped || has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
	// TODO: handle the remaining flags + host ptr
	
	// zero init cuda textures array
	textures.fill(0);
	
	// need to allocate the buffer on the correct device, if a context was specified,
	// else: assume the correct context is already active
	const auto& cuda_dev = (const cuda_device&)cqueue.get_device();
	if(cuda_dev.ctx != nullptr) {
		CU_CALL_RET(cu_ctx_set_current(cuda_dev.ctx),
					"failed to make cuda context current")
	}
	
	// check Vulkan image sharing validity
	if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
#if defined(FLOOR_NO_VULKAN)
		log_error("Vulkan support is not enabled");
		return;
#else
		if (!cuda_can_use_external_memory()) {
			log_error("can't use Vulkan image sharing, because use of external memory is not supported");
			return;
		}
#endif
	}
	
	// actually create the image
	if(!create_internal(true, cqueue)) {
		return; // can't do much else
	}
}

bool cuda_image::create_internal(const bool copy_host_data, const compute_queue& cqueue) REQUIRES(!device_sampler_mtx) {
	// image handling in cuda/ptx is somewhat complicated:
	// when using a texture object, you can only read from it, but with sampler support,
	// when using a surface object, you can read _and_ write from/to it, but without sampler support.
	// if write-only, only use surfaces
	const bool write_only { !has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type) && has_flag<COMPUTE_IMAGE_TYPE::WRITE>(image_type) };
	const bool need_tex { has_flag<COMPUTE_MEMORY_FLAG::READ>(flags) && !write_only };
	const bool need_surf { has_flag<COMPUTE_MEMORY_FLAG::WRITE>(flags) || write_only };
	
	//
	const auto dim_count = image_dim_count(image_type);
	const auto is_compressed = image_compressed(image_type);
	const auto is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type);
	const auto is_cube = has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type);
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
	
	// 3D depth or #layers (including cube map faces)
	const uint32_t depth = (dim_count >= 3 ? image_dim.z :
							(is_array || is_cube ? layer_count : 0));
	if(is_cube) {
		// make sure width == height
		if(image_dim.x != image_dim.y) {
			log_error("cube map side width and height must be equal ($ != $)!", image_dim.x, image_dim.y);
			return false;
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
		log_error("unsupported image format: $ ($X)", image_type_to_string(image_type), image_type);
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
	const cu_array_3d_descriptor array_desc {
		.dim = {
			image_dim.x,
			(dim_count >= 2 ? image_dim.y : 0),
			depth
		},
		.format = format,
		.channel_count = channel_count,
		.flags = (
				  (is_array ? CU_ARRAY_3D_FLAGS::LAYERED : CU_ARRAY_3D_FLAGS::NONE) |
				  (is_cube ? CU_ARRAY_3D_FLAGS::CUBE_MAP : CU_ARRAY_3D_FLAGS::NONE) |
				  // NOTE: depth flag is not supported and array creation will return INVALID_VALUE
				  // (has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type) ? CU_ARRAY_3D_FLAGS::DEPTH_TEXTURE : CU_ARRAY_3D_FLAGS::NONE) |
				  (has_flag<COMPUTE_IMAGE_TYPE::FLAG_GATHER>(image_type) ? CU_ARRAY_3D_FLAGS::TEXTURE_GATHER : CU_ARRAY_3D_FLAGS::NONE) |
				  (need_surf ? CU_ARRAY_3D_FLAGS::SURFACE_LOAD_STORE : CU_ARRAY_3D_FLAGS::NONE)
		)
	};
	if(!has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags) &&
	   !has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		log_debug("surf/tex $/$; dim $: $; channels $; flags $; format: $X",
				  need_surf, need_tex, dim_count, array_desc.dim, array_desc.channel_count, array_desc.flags, array_desc.format);
		if(!is_mip_mapped) {
			CU_CALL_RET(cu_array_3d_create(&image_array, &array_desc),
						"failed to create cuda array/image", false)
			image = image_array;
		}
		else {
			CU_CALL_RET(cu_mipmapped_array_create(&image_mipmap_array, &array_desc, mip_level_count),
						"failed to create cuda mip-mapped array/image", false)
			image = image_mipmap_array;
			
			image_mipmap_arrays.resize(mip_level_count);
			for(uint32_t level = 0; level < mip_level_count; ++level) {
				CU_CALL_RET(cu_mipmapped_array_get_level(&image_mipmap_arrays[level], image_mipmap_array, level),
							"failed to retrieve cuda mip-map level #" + to_string(level), false)
			}
		}
		
		// copy host memory to device if it is non-null and NO_INITIAL_COPY is not specified
		if(copy_host_data && host_data.data() != nullptr && !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
			log_debug("copying $ bytes from $X to array $X",
					  image_data_size, host_data.data(), image);
			auto cpy_host_data = host_data;
			apply_on_levels([this, &cpy_host_data](const uint32_t& level,
												   const uint4& mip_image_dim,
												   const uint32_t& slice_data_size,
												   const uint32_t& level_data_size) {
				if (!cuda_memcpy<CU_MEMORY_TYPE::HOST, CU_MEMORY_TYPE::ARRAY>(cpy_host_data.data(),
																			  (is_mip_mapped ? image_mipmap_arrays[level] : image_array),
																			  slice_data_size / max(mip_image_dim.y, 1u),
																			  mip_image_dim.y, mip_image_dim.z * layer_count)) {
					log_error("failed to copy initial host data to device");
					return false;
				}
				cpy_host_data = cpy_host_data.subspan(level_data_size, cpy_host_data.size_bytes() - level_data_size);
				return true;
			});
		}
	}
	// -> Vulkan image
	else if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
#if !defined(FLOOR_NO_VULKAN)
		if (!create_shared_vulkan_image(copy_host_data)) {
			return false;
		}
		
		// import
		const auto vk_image_size = shared_vk_image->get_vulkan_allocation_size();
		if (vk_image_size < image_data_size) {
			log_error("Vulkan image allocation size ($) is smaller than the specified CUDA image size ($)",
					  vk_image_size, image_data_size);
			return false;
		}
		cu_external_memory_handle_descriptor ext_mem_desc {
#if defined(__WINDOWS__)
			.type = (core::is_windows_8_or_higher() ?
					 CU_EXTERNAL_MEMORY_HANDLE_TYPE::OPAQUE_WIN32 :
					 CU_EXTERNAL_MEMORY_HANDLE_TYPE::OPAQUE_WIN32_KMT),
			.handle.win32 = {
				.handle = shared_vk_image->get_vulkan_shared_handle(),
				.name = nullptr,
			},
#else
			.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE::OPAQUE_FD,
			.handle.fd = shared_vk_image->get_vulkan_shared_handle(),
#endif
			.size = vk_image_size,
			.flags = 0, // not relevant for Vulkan
		};
		CU_CALL_RET(cu_import_external_memory(&ext_memory, &ext_mem_desc),
					"failed to import external Vulkan image", false)
		
		// map
		// NOTE: CUDA considers the image/array to always be mip-mapped (even if it only has one level)
		cu_external_memory_mip_mapped_array_descriptor ext_array_desc {
			.offset = 0,
			.array_desc = array_desc,
			.num_levels = mip_level_count,
		};
		if (has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type)) {
			ext_array_desc.array_desc.flags |= CU_ARRAY_3D_FLAGS::DEPTH_TEXTURE;
		}
		if (!has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type) &&
			has_flag<COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET>(image_type)) {
			ext_array_desc.array_desc.flags |= CU_ARRAY_3D_FLAGS::COLOR_ATTACHMENT;
		}
		CU_CALL_RET(cu_external_memory_get_mapped_mip_mapped_array(&image_mipmap_array, ext_memory, &ext_array_desc),
					"failed to get mapped array/image pointer from external Vulkan image", false)
		image = image_mipmap_array;
		
		image_mipmap_arrays.resize(mip_level_count);
		for(uint32_t level = 0; level < mip_level_count; ++level) {
			CU_CALL_RET(cu_mipmapped_array_get_level(&image_mipmap_arrays[level], image_mipmap_array, level),
						"failed to retrieve cuda mip-map level #" + to_string(level), false)
		}
#else
		return false; // no Vulkan support
#endif
	}
	// -> OpenGL image
	else {
		if(!create_gl_image(copy_host_data)) return false;
		log_debug("surf/tex $/$", need_surf, need_tex);
		
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
					log_error("can't share opengl depth format $X with cuda", gl_internal_format);
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
				if(is_array || is_cube ||
				   has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type)) {
					log_error("unsupported depth image format ($X), only 2D depth or depth+stencil is supported!",
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
					log_error("depth compat fbo/tex error: $X $X", err, fbo_err);
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
		if(need_surf) cuda_gl_flags |= CU_GRAPHICS_REGISTER_FLAGS::SURFACE_LOAD_STORE;
		CU_CALL_RET(cu_graphics_gl_register_image(&rsrc,
												  (depth_compat_tex == 0 ? gl_object : depth_compat_tex),
												  opengl_type, cuda_gl_flags),
					"failed to register opengl image with cuda", false)
		if(rsrc == nullptr) {
			log_error("created cuda gl graphics resource is invalid!");
			return false;
		}
		// acquire for use with cuda
		acquire_opengl_object(&cqueue);
	}
	
	// create texture/surface objects, depending on read/write flags and sampler support
	cu_resource_descriptor rsrc_desc;
	cu_resource_view_descriptor rsrc_view_desc;
	memset(&rsrc_desc, 0, sizeof(cu_resource_descriptor));
	memset(&rsrc_view_desc, 0, sizeof(cu_resource_view_descriptor));
	
	// TODO: support LINEAR/PITCH2D?
	if(is_mip_mapped_or_vulkan) {
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
		rsrc_view_desc.last_layer = (!is_cube ? layer_count : layer_count / 6) - 1;
		
		for(uint32_t i = 0, count = uint32_t(size(textures)); i < count; ++i) {
			cu_texture_descriptor tex_desc;
			memset(&tex_desc, 0, sizeof(cu_texture_descriptor));
			
			// address mode (either clamp-to-edge or repeat/wrap)
			const auto sampler_addr_mode = cuda_sampler::get_address_mode(i);
			const auto address_mode = (sampler_addr_mode == cuda_sampler::REPEAT ? CU_ADDRESS_MODE::WRAP :
									   (sampler_addr_mode == cuda_sampler::REPEAT_MIRRORED ? CU_ADDRESS_MODE::MIRROR :
										CU_ADDRESS_MODE::CLAMP));
			tex_desc.address_mode[0] = address_mode;
			if(dim_count >= 2) tex_desc.address_mode[1] = address_mode;
			if(dim_count >= 3) tex_desc.address_mode[2] = address_mode;
			
			// filter mode
			const auto filter_mode = (cuda_sampler::get_filter_mode(i) == cuda_sampler::NEAREST ?
									  CU_FILTER_MODE::NEAREST : CU_FILTER_MODE::LINEAR);
			tex_desc.filter_mode = filter_mode;
			tex_desc.mip_map_filter_mode = filter_mode;
			
			// non-normalized / normalized coordinates
			const auto coord_mode = (cuda_sampler::get_coord_mode(i) == cuda_sampler::PIXEL ?
									 CU_TEXTURE_FLAGS::NONE : CU_TEXTURE_FLAGS::NORMALIZED_COORDINATES);
			tex_desc.flags = coord_mode;
			
			tex_desc.max_anisotropy = image_anisotropy(image_type);
			tex_desc.min_mip_map_level_clamp = 0.0f;
			tex_desc.max_mip_map_level_clamp = (is_mip_mapped_or_vulkan ? float(dev.max_mip_levels) : 0.0f);
			
			// at this point, the device function pointer that initializes/creates the sampler state
			// has been overwritten/hijacked by our own function (if the internal api is used/enabled)
			// -> set the sampler state that we want to have
			GUARD(device_sampler_mtx); // necessary, b/c we don't know which device is calling us in internal_device_sampler_init
			cur_device = (const cuda_device*)&dev;
			cuda_sampler_or.low = 0;
			cuda_sampler_or.high = 0;
			// this is no longer exhaustive
			auto compare_function = cuda_sampler::COMPARE_FUNCTION::NONE;
			if (((i & cuda_sampler::COMPARE_FUNCTION_MASK) >> cuda_sampler::COMPARE_FUNCTION_SHIFT) <= cuda_sampler::COMPARE_FUNCTION_MAX) {
				compare_function = cuda_sampler::get_compare_function(i);
				if (compare_function != cuda_sampler::NONE) {
					apply_sampler_modifications = true;
					switch (compare_function) {
						case cuda_sampler::LESS:
							cuda_sampler_or.compare_function = CU_SAMPLER_TYPE::COMPARE_FUNCTION::LESS;
							break;
						case cuda_sampler::LESS_OR_EQUAL:
							cuda_sampler_or.compare_function = CU_SAMPLER_TYPE::COMPARE_FUNCTION::LESS_OR_EQUAL;
							break;
						case cuda_sampler::GREATER:
							cuda_sampler_or.compare_function = CU_SAMPLER_TYPE::COMPARE_FUNCTION::GREATER;
							break;
						case cuda_sampler::GREATER_OR_EQUAL:
							cuda_sampler_or.compare_function = CU_SAMPLER_TYPE::COMPARE_FUNCTION::GREATER_OR_EQUAL;
							break;
						case cuda_sampler::EQUAL:
							cuda_sampler_or.compare_function = CU_SAMPLER_TYPE::COMPARE_FUNCTION::EQUAL;
							break;
						case cuda_sampler::NOT_EQUAL:
							cuda_sampler_or.compare_function = CU_SAMPLER_TYPE::COMPARE_FUNCTION::NOT_EQUAL;
							break;
						default: break;
					}
				}
			}
			
			cu_tex_object new_texture = 0;
			CU_CALL_RET(cu_tex_object_create(&new_texture, &rsrc_desc, &tex_desc, &rsrc_view_desc),
						"failed to create texture object #" + to_string(i), false)
			// we can do this, because cuda only tracks/returns the lower 32-bit of cu_tex_object
			textures[i] = (cu_tex_only_object)new_texture;
			
			// cleanup
			apply_sampler_modifications = false;
		}
	}
	if(need_surf) {
		// there is no mip-map surface equivalent, so we must create a surface for each mip-map level from each level array
		if(is_mip_mapped_or_vulkan) {
			rsrc_desc.type = CU_RESOURCE_TYPE::ARRAY;
		}
		
		surfaces.resize(mip_level_count);
		for(uint32_t level = 0; level < mip_level_count; ++level) {
			if(is_mip_mapped_or_vulkan) {
				rsrc_desc.array = image_mipmap_arrays[level];
			}
			CU_CALL_RET(cu_surf_object_create(&surfaces[level], &rsrc_desc),
						"failed to create surface object", false)
		}
		
		// since we don't want to carry around 64-bit values for all possible mip-levels for all images (15 * 8 == 120 bytes per image!),
		// store all mip-map level surface "objects"/ids in a separate buffer, which we will access if lod write is actually being used
		if (is_mip_mapped_or_vulkan) {
			std::span<uint8_t> surfaces_data { (uint8_t*)&surfaces[0], surfaces.size() * sizeof(typename decltype(surfaces)::value_type) };
			surfaces_lod_buffer = make_shared<cuda_buffer>(cqueue, surfaces_data.size_bytes(), surfaces_data,
														   COMPUTE_MEMORY_FLAG::READ | COMPUTE_MEMORY_FLAG::HOST_WRITE);
		}
	}
	else {
		// create dummy surface object (needed when setting kernel args)
		surfaces.resize(1);
		surfaces[0] = 0;
	}
	
	// manually create mip-map chain
	if(generate_mip_maps &&
	   // when using gl sharing: just acquired the opengl image, so no need to do this
	   !has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags)) {
		generate_mip_map_chain(cqueue);
	}
	
	return true;
}

cuda_image::~cuda_image() {
	// kill the image
	
	for(const auto& texture : textures) {
		if(texture != 0) {
			CU_CALL_NO_ACTION(cu_tex_object_destroy(texture),
							  "failed to destroy texture object")
		}
	}
	for(const auto& surface : surfaces) {
		if(surface != 0) {
			CU_CALL_NO_ACTION(cu_surf_object_destroy(surface),
							  "failed to destroy surface object")
		}
	}
	
	// -> cuda array
	if (!has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags) &&
		!has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		if (image_array != nullptr) {
			CU_CALL_RET(cu_array_destroy(image_array),
						"failed to free device memory")
		}
		if (image_mipmap_array != nullptr) {
			CU_CALL_RET(cu_mipmapped_array_destroy(image_mipmap_array),
						"failed to free device memory")
		}
	}
#if !defined(FLOOR_NO_VULKAN)
	// -> Vulkan image
	else if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		if (image_mipmap_array != nullptr) {
			// CUDA doc says that shared/external memory must also be freed
			CU_CALL_RET(cu_mipmapped_array_destroy(image_mipmap_array),
						"failed to free shared external memory")
		}
		if (ext_memory != nullptr) {
			CU_CALL_IGNORE(cu_destroy_external_memory(ext_memory), "failed to destroy shared external memory")
		}
		cuda_vk_image = nullptr;
		if (ext_sema != nullptr) {
			CU_CALL_IGNORE(cu_destroy_external_semaphore(ext_sema), "failed to destroy shared external semaphore")
		}
		cuda_vk_sema = nullptr;
	}
#endif
	// -> OpenGL image
	else {
		if (gl_object == 0) {
			log_error("invalid opengl image!");
		} else {
			if (image == nullptr || gl_object_state) {
				log_warn("image still registered for opengl use - acquire before destructing a compute image!");
			}
			// kill opengl image
			if (!gl_object_state) release_opengl_object(nullptr); // -> release to opengl
			delete_gl_image();
		}
	}
	
	// clean up depth compat objects
	if (depth_copy_fbo != 0) {
		glDeleteFramebuffers(1, &depth_copy_fbo);
	}
	if (depth_compat_tex != 0) {
		glDeleteTextures(1, &depth_compat_tex);
	}
}

bool cuda_image::zero(const compute_queue& cqueue) {
	if(image == nullptr) return false;
	
	// NOTE: when using mip-mapping, we can reuse the zero data ptr from the first level (all levels will be smaller than the first)
	const auto first_level_size = image_data_size_from_types(image_dim, image_type, true);
	auto zero_data = make_unique<uint8_t[]>(first_level_size);
	auto zero_data_ptr = zero_data.get();
	memset(zero_data_ptr, 0, first_level_size);
	
	const auto success = apply_on_levels<true>([this, &zero_data_ptr](const uint32_t& level,
																	  const uint4& mip_image_dim,
																	  const uint32_t& slice_data_size,
																	  const uint32_t&) {
		if(!cuda_memcpy<CU_MEMORY_TYPE::HOST, CU_MEMORY_TYPE::ARRAY>(zero_data_ptr,
																	 (is_mip_mapped_or_vulkan ? image_mipmap_arrays[level] : image_array),
																	 slice_data_size / max(mip_image_dim.y, 1u),
																	 mip_image_dim.y, mip_image_dim.z * layer_count)) {
			log_error("failed to zero image");
			return false;
		}
		return true;
	});
	
	cqueue.finish();
	
	return success;
}

void* __attribute__((aligned(128))) cuda_image::map(const compute_queue& cqueue, const COMPUTE_MEMORY_MAP_FLAG flags_) {
	if(image == nullptr) return nullptr;
	
	// TODO: parameter origin + region
	// NOTE: a) not supported with mip-mapping if region != image size, b) must update all map/unmap code (relies on region == image size right now)
	const auto map_size = image_data_size;
	
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
	auto host_buffer = make_aligned_ptr<uint8_t>(map_size);
	
	// check if we need to copy the image from the device (in case READ was specified)
	if(!write_only) {
		if(blocking_map) {
			// must finish up all current work before we can properly read from the current buffer
			cqueue.finish();
		}
		
		auto cpy_host_ptr = host_buffer.get();
		apply_on_levels([this, &cpy_host_ptr, &blocking_map,
						 stream = (const_cu_stream)cqueue.get_queue_ptr()](const uint32_t& level,
																		   const uint4& mip_image_dim,
																		   const uint32_t& slice_data_size,
																		   const uint32_t& level_data_size) {
			if(!cuda_memcpy<CU_MEMORY_TYPE::ARRAY, CU_MEMORY_TYPE::HOST>(cpy_host_ptr,
																		 (is_mip_mapped_or_vulkan ? image_mipmap_arrays[level] : image_array),
																		 slice_data_size / max(mip_image_dim.y, 1u),
																		 mip_image_dim.y, mip_image_dim.z * layer_count,
																		 !blocking_map, stream)) {
				log_error("failed to copy device memory to host");
				return false;
			}
			cpy_host_ptr += level_data_size;
			return true;
		});
	}
	
	// need to remember how much we mapped and where (so the host->device copy copies the right amount of bytes)
	auto ret_ptr = host_buffer.get();
	mappings.emplace(ret_ptr, cuda_mapping { std::move(host_buffer), flags_ });
	
	return ret_ptr;
}

bool cuda_image::unmap(const compute_queue& cqueue,
					   void* __attribute__((aligned(128))) mapped_ptr) {
	if(image == nullptr) return false;
	if(mapped_ptr == nullptr) return false;
	
	// check if this is actually a mapped pointer (+get the mapped size)
	const auto iter = mappings.find(mapped_ptr);
	if(iter == mappings.end()) {
		log_error("invalid mapped pointer: $X", mapped_ptr);
		return false;
	}
	
	// check if we need to actually copy data back to the device (not the case if read-only mapping)
	bool success = true;
	if (has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE>(iter->second.flags) ||
		has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(iter->second.flags)) {
		auto cpy_host_ptr = (uint8_t*)mapped_ptr;
		success = apply_on_levels([this, &cpy_host_ptr](const uint32_t& level,
														const uint4& mip_image_dim,
														const uint32_t& slice_data_size,
														const uint32_t& level_data_size) {
			if(!cuda_memcpy<CU_MEMORY_TYPE::HOST, CU_MEMORY_TYPE::ARRAY>(cpy_host_ptr,
																		 (is_mip_mapped_or_vulkan ? image_mipmap_arrays[level] : image_array),
																		 slice_data_size / max(mip_image_dim.y, 1u),
																		 mip_image_dim.y, mip_image_dim.z * layer_count)) {
				log_error("failed to copy host memory to device");
				return false;
			}
			cpy_host_ptr += level_data_size;
			return true;
		});
		
		// update mip-map chain
		if(generate_mip_maps) {
			generate_mip_map_chain(cqueue);
		}
	}
	
	// free host memory again and remove the mapping
	mappings.erase(mapped_ptr);
	
	return success;
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

bool cuda_image::acquire_opengl_object(const compute_queue* cqueue) {
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
										  (cqueue != nullptr ? (const_cu_stream)cqueue->get_queue_ptr() : nullptr)),
				"failed to acquire opengl image - cuda resource mapping failed!", false)
	gl_object_state = false;
	
	// TODO: handle opengl array/layers
	if(is_mip_mapped) {
		CU_CALL_RET(cu_graphics_resource_get_mapped_mipmapped_array(&image_mipmap_array, rsrc),
					"failed to retrieve mapped cuda mip-map image from opengl image!", false)
		image = image_mipmap_array;
		
		image_mipmap_arrays.resize(mip_level_count);
		for(uint32_t level = 0; level < mip_level_count; ++level) {
			CU_CALL_RET(cu_graphics_sub_resource_get_mapped_array(&image_mipmap_arrays[level], rsrc, 0, level),
						"failed to retrieve mip-map level #" + to_string(level) + " from mapped opengl image!", false)
		}
	}
	else {
		CU_CALL_RET(cu_graphics_sub_resource_get_mapped_array(&image_array, rsrc, 0, 0),
					"failed to retrieve mapped cuda image from opengl image!", false)
		image = image_array;
	}
	
	if(image == nullptr) {
		log_error("mapped cuda image (from a graphics resource) is invalid!");
		return false;
	}
	
	return true;
}

bool cuda_image::release_opengl_object(const compute_queue* cqueue) {
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
											(cqueue != nullptr ? (const_cu_stream)cqueue->get_queue_ptr() : nullptr)),
				"failed to release opengl image - cuda resource unmapping failed!", false)
	gl_object_state = true;
	
	return true;
}
	
#if !defined(FLOOR_NO_VULKAN)
bool cuda_image::create_shared_vulkan_image(const bool copy_host_data) {
	const vulkan_compute* vk_render_ctx = nullptr;
	const compute_device* render_dev = nullptr;
	if (shared_vk_image == nullptr || cuda_vk_image != nullptr /* !nullptr if resize */ || !cuda_vk_sema) {
		// get the render/graphics context so that we can create a image (TODO: allow specifying a different context?)
		auto render_ctx = floor::get_render_context();
		if (render_ctx->get_compute_type() != COMPUTE_TYPE::VULKAN) {
			log_error("CUDA/Vulkan image sharing failed: render context is not Vulkan");
			return false;
		}
		vk_render_ctx = (const vulkan_compute*)render_ctx.get();
		
		// get the device and its default queue where we want to create the image on/in
		render_dev = vk_render_ctx->get_corresponding_device(dev);
		if (render_dev == nullptr) {
			log_error("CUDA/Vulkan image sharing failed: failed to find a matching Vulkan device");
			return false;
		}
	}
	
	if (shared_vk_image == nullptr || cuda_vk_image != nullptr /* !nullptr if resize */) {
		// create the underlying Vulkan image
		auto default_queue = vk_render_ctx->get_device_default_queue(*render_dev);
		auto shared_vk_image_flags = flags;
		if (!copy_host_data) {
			shared_vk_image_flags |= COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY;
		}
		cuda_vk_image = vk_render_ctx->create_image(*default_queue, image_dim, image_type, host_data, shared_vk_image_flags);
		cuda_vk_image->set_debug_label("cuda_vk_image");
		if (!cuda_vk_image) {
			log_error("CUDA/Vulkan image sharing failed: failed to create the underlying shared Vulkan image");
			return false;
		}
		shared_vk_image = (vulkan_image*)cuda_vk_image.get();
	}
	// else: wrapping an existing Vulkan image
	
	const auto vk_shared_handle = shared_vk_image->get_vulkan_shared_handle();
	if (
#if defined(__WINDOWS__)
		vk_shared_handle == nullptr
#else
		vk_shared_handle == 0
#endif
		) {
		log_error("shared Vulkan image has no shared memory handle");
		return false;
	}
	
	// create the sync sema (note that we only need to create this once)
	if (!cuda_vk_sema) {
		cuda_vk_sema = make_unique<vulkan_semaphore>(*render_dev, true /* external */);
		auto& vk_sema = cuda_vk_sema->get_semaphore();
		if (vk_sema == nullptr) {
			log_error("CUDA/Vulkan image sharing failed: failed to create sync semaphore");
			return false;
		}
		
		cu_external_semaphore_handle_descriptor ext_sema_desc {
#if defined(__WINDOWS__)
			.type = (core::is_windows_8_or_higher() ?
					 CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE::OPAQUE_WIN32 :
					 CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE::OPAQUE_WIN32_KMT),
			.handle.win32 = {
				.handle = cuda_vk_sema->get_shared_handle(),
				.name = nullptr,
			},
#else
			.type = CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE::OPAQUE_FD,
			.handle.fd = cuda_vk_sema->get_shared_handle(),
#endif
			.flags = 0, // not relevant for Vulkan
		};
		CU_CALL_RET(cu_import_external_semaphore(&ext_sema, &ext_sema_desc),
					"failed to import external Vulkan semaphore", false)
	}
	
	return true;
}
#endif

#if !defined(FLOOR_NO_VULKAN)
bool cuda_image::acquire_vulkan_image(const compute_queue& cqueue floor_unused_if_release, const vulkan_queue& vk_queue) {
	if (!vk_object_state) {
#if defined(FLOOR_DEBUG)
		log_warn("Vulkan image has already been acquired for use with CUDA!");
#endif
		return true;
	}
	
	// validate CUDA queue
#if defined(FLOOR_DEBUG)
	if (const auto cuda_queue_ = dynamic_cast<const cuda_queue*>(&cqueue); cuda_queue_ == nullptr) {
		log_error("specified queue is not a CUDA queue");
		return false;
	}
#endif
	
	// finish Vulkan queue
	vk_queue.finish();
	vk_object_state = false;
	return true;
}

bool cuda_image::release_vulkan_image(const compute_queue& cqueue, const vulkan_queue&) {
	if (vk_object_state) {
#if defined(FLOOR_DEBUG)
		log_warn("Vulkan image has already been released for Vulkan use!");
#endif
		return true;
	}
	
	// validate CUDA queue
#if defined(FLOOR_DEBUG)
	if (const auto cuda_queue_ = dynamic_cast<const cuda_queue*>(&cqueue); cuda_queue_ == nullptr) {
		log_error("specified queue is not a CUDA queue");
		return false;
	}
#endif
	
	// finish CUDA queue
	cqueue.finish();
	vk_object_state = true;
	return true;
}
#else
bool cuda_image::acquire_vulkan_image(const compute_queue&, const vulkan_queue&) {
	return false;
}
bool cuda_image::release_vulkan_image(const compute_queue&, const vulkan_queue&) {
	return false;
}
#endif

#endif
