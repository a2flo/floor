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

#include <floor/floor/floor.hpp>
#include <floor/compute/compute_image.hpp>
#include <floor/compute/compute_device.hpp>
#include <floor/compute/compute_context.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/core/logger.hpp>

safe_mutex compute_image::minify_programs_mtx;
unordered_map<compute_context*, unique_ptr<compute_image::minify_program>> compute_image::minify_programs;

compute_image::~compute_image() {}

#if !defined(FLOOR_IOS)
void compute_image::delete_gl_image() {
	if(has_external_gl_object) return;
	if(gl_object == 0) return;
	glDeleteTextures(1, &gl_object);
	gl_object = 0;
}

bool compute_image::create_gl_image(const bool copy_host_data) {
	if(has_external_gl_object) return true;
	
	// clean up previous gl image that might still exist
	delete_gl_image();
	
	// create and init the opengl image
	glGenTextures(1, &gl_object);
	glBindTexture(opengl_type, gl_object);
	if(gl_object == 0 || !glIsTexture(gl_object)) {
		log_error("created opengl image %u is invalid!", gl_object);
		return false;
	}
	
	const auto storage_dim_count = image_storage_dim_count(image_type);
	const auto channel_count = image_channel_count(image_type);
	const auto data_type = (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
	const auto image_format = (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK);
	
	glTexParameteri(opengl_type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(opengl_type, GL_TEXTURE_MIN_FILTER, !is_mip_mapped ? GL_NEAREST : GL_NEAREST_MIPMAP_NEAREST);
	glTexParameterf(opengl_type, GL_TEXTURE_MIN_LOD, 0.0f);
	glTexParameterf(opengl_type, GL_TEXTURE_MAX_LOD, !is_mip_mapped ? 0.0f : (float)mip_level_count);
	glTexParameteri(opengl_type, GL_TEXTURE_MAX_LEVEL, !is_mip_mapped ? 0 : int(mip_level_count - 1));
	
	glTexParameteri(opengl_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	if(storage_dim_count >= 2) {
		glTexParameteri(opengl_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	if(storage_dim_count >= 3) {
		glTexParameteri(opengl_type, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	}
	
	// TODO: only do this for depth textures?
	glTexParameteri(opengl_type, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	
	// init texture data
	void* pixel_ptr = (copy_host_data && host_ptr != nullptr &&
					   !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags) ?
					   host_ptr : nullptr);
	
	GLint internal_format = 0;
	GLenum format = 0;
	GLenum type = 0;
	if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type)) {
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_STENCIL>(image_type)) {
			format = GL_DEPTH_STENCIL;
			switch(image_format) {
				case COMPUTE_IMAGE_TYPE::FORMAT_24:
					internal_format = GL_DEPTH24_STENCIL8;
					type = GL_UNSIGNED_INT_24_8;
					break;
				case COMPUTE_IMAGE_TYPE::FORMAT_32_8:
					if(data_type != COMPUTE_IMAGE_TYPE::FLOAT) {
						log_error("data type of FORMAT_32_8 must be FLOAT!");
						return false;
					}
					internal_format = GL_DEPTH32F_STENCIL8;
					type = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
					break;
				default:
					log_error("format not supported for depth image: %X", image_format);
					return false;
			}
		}
		else {
			format = GL_DEPTH_COMPONENT;
			switch(image_format) {
				case COMPUTE_IMAGE_TYPE::FORMAT_16:
					internal_format = GL_DEPTH_COMPONENT16;
					type = GL_UNSIGNED_SHORT;
					break;
				case COMPUTE_IMAGE_TYPE::FORMAT_24:
					internal_format = GL_DEPTH_COMPONENT24;
					type = GL_UNSIGNED_INT;
					break;
				case COMPUTE_IMAGE_TYPE::FORMAT_32:
					if(data_type == COMPUTE_IMAGE_TYPE::FLOAT) {
						internal_format = GL_DEPTH_COMPONENT32F;
						type = GL_FLOAT;
					}
					else {
						internal_format = GL_DEPTH_COMPONENT32;
						type = GL_UNSIGNED_INT;
					}
					break;
				default:
					log_error("format not supported for depth image: %X", image_format);
					return false;
			}
		}
	}
	else {
		if(channel_count == 1) internal_format = GL_RED;
		else if(channel_count == 2) internal_format = GL_RG;
		else if(channel_count == 3) internal_format = GL_RGB;
		else internal_format = GL_RGBA;
		format = (GLenum)internal_format;
		
		// TODO: use sized types/formats -> better do this via a lookup table { data_type, channel count, format } -> { gl types ... }
		
		switch(data_type) {
			case COMPUTE_IMAGE_TYPE::UINT:
				switch(image_format) {
					case COMPUTE_IMAGE_TYPE::FORMAT_2:
						type = GL_UNSIGNED_BYTE;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_4:
						type = GL_UNSIGNED_SHORT_4_4_4_4;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_8:
						type = GL_UNSIGNED_BYTE;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_16:
						type = GL_UNSIGNED_SHORT;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_32:
						type = GL_UNSIGNED_INT;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_3_3_2:
						type = GL_UNSIGNED_BYTE_3_3_2;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5:
						type = GL_UNSIGNED_SHORT;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5_ALPHA_1:
						type = GL_UNSIGNED_SHORT_5_5_5_1;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_5_6_5:
						type = GL_UNSIGNED_SHORT_5_6_5;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_10:
						type = GL_UNSIGNED_INT;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_10_10_10_ALPHA_2:
						type = GL_UNSIGNED_INT_10_10_10_2;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12:
						type = GL_UNSIGNED_INT;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12_12:
						type = GL_UNSIGNED_INT;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_24:
						type = GL_UNSIGNED_INT;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_24_8:
						type = GL_UNSIGNED_INT_24_8;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_9_9_9_EXP_5:
					case COMPUTE_IMAGE_TYPE::FORMAT_64:
					case COMPUTE_IMAGE_TYPE::FORMAT_11_11_10:
					case COMPUTE_IMAGE_TYPE::FORMAT_32_8:
						log_error("format not supported for unsigned data type: %X", image_format);
						return false;
					default:
						log_error("unknown format: %X", image_format);
						return false;
				}
				break;
			case COMPUTE_IMAGE_TYPE::INT:
				switch(image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) {
					case COMPUTE_IMAGE_TYPE::FORMAT_2:
						type = GL_BYTE;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_8:
						type = GL_BYTE;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_16:
						type = GL_SHORT;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_32:
						type = GL_INT;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_4:
					case COMPUTE_IMAGE_TYPE::FORMAT_64:
					case COMPUTE_IMAGE_TYPE::FORMAT_3_3_2:
					case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5:
					case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5_ALPHA_1:
					case COMPUTE_IMAGE_TYPE::FORMAT_5_6_5:
					case COMPUTE_IMAGE_TYPE::FORMAT_9_9_9_EXP_5:
					case COMPUTE_IMAGE_TYPE::FORMAT_10:
					case COMPUTE_IMAGE_TYPE::FORMAT_10_10_10_ALPHA_2:
					case COMPUTE_IMAGE_TYPE::FORMAT_11_11_10:
					case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12:
					case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12_12:
					case COMPUTE_IMAGE_TYPE::FORMAT_24:
					case COMPUTE_IMAGE_TYPE::FORMAT_24_8:
					case COMPUTE_IMAGE_TYPE::FORMAT_32_8:
						log_error("format not supported for signed data type: %X", image_format);
						return false;
					default:
						log_error("unknown format: %X", image_format);
						return false;
				}
				break;
			case COMPUTE_IMAGE_TYPE::FLOAT:
				switch(image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) {
					case COMPUTE_IMAGE_TYPE::FORMAT_16:
						type = GL_HALF_FLOAT;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_32:
						type = GL_FLOAT;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_64:
						type = GL_DOUBLE;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_9_9_9_EXP_5:
						type = GL_UNSIGNED_INT_5_9_9_9_REV;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_11_11_10:
						type = GL_UNSIGNED_INT_10F_11F_11F_REV;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_2:
					case COMPUTE_IMAGE_TYPE::FORMAT_4:
					case COMPUTE_IMAGE_TYPE::FORMAT_8:
					case COMPUTE_IMAGE_TYPE::FORMAT_3_3_2:
					case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5:
					case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5_ALPHA_1:
					case COMPUTE_IMAGE_TYPE::FORMAT_5_6_5:
					case COMPUTE_IMAGE_TYPE::FORMAT_10:
					case COMPUTE_IMAGE_TYPE::FORMAT_10_10_10_ALPHA_2:
					case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12:
					case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12_12:
					case COMPUTE_IMAGE_TYPE::FORMAT_24:
					case COMPUTE_IMAGE_TYPE::FORMAT_24_8:
					case COMPUTE_IMAGE_TYPE::FORMAT_32_8:
						log_error("format not supported for float data type: %X", image_format);
						return false;
					default:
						log_error("unknown format: %X", image_format);
						return false;
				}
				break;
			default:
				log_error("unknown data type: %X", data_type);
				return false;
		}
	}
	
	if((image_type & COMPUTE_IMAGE_TYPE::__DIM_MASK) == COMPUTE_IMAGE_TYPE::NONE) {
		log_error("dimension not set!");
		return false;
	}
	
	// if everything has been successful, store the used gl types for later use
	gl_internal_format = internal_format;
	gl_format = format;
	gl_type = type;
	
	// create gl texture (and init with host data if pixel_ptr != nullptr)
	init_gl_image_data(pixel_ptr);
	glBindTexture(opengl_type, 0);
	
	return true;
}

void compute_image::init_gl_image_data(const void* data) {
	const GLsizei sample_count { 1 }; // TODO: support this properly
	const bool fixed_sample_locations { false }; // TODO: support this properly
	const auto dim_count = image_dim_count(image_type);
	const auto storage_dim_count = image_storage_dim_count(image_type);
	const auto is_cube = has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type);
	const auto is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type);
	
	// upload level count == level count (if provided by user), or 1 if we have to manually generate the mip levels
	const auto upload_level_count = GLint(generate_mip_maps ? 1 : mip_level_count);
	const void* level_data = data;
	int4 mip_image_dim {
		(int)image_dim.x,
		dim_count >= 2 ? (int)image_dim.y : 0,
		dim_count >= 3 ? (int)image_dim.z : 0,
		0
	};
	for(GLint level = 0; level < upload_level_count; ++level, mip_image_dim >>= 1) {
		const auto slice_data_size = image_slice_data_size_from_types(mip_image_dim, image_type, sample_count);
		const auto level_data_size = slice_data_size * layer_count;
		
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_BUFFER>(image_type)) {
			// TODO: how to init this?
		}
		else if(is_cube) {
			if(!is_array) {
				static constexpr const GLenum cube_targets[] {
					GL_TEXTURE_CUBE_MAP_POSITIVE_X,
					GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
					GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
					GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
					GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
					GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
				};
				
				auto cube_data = (data != nullptr ? level_data : nullptr);
				for(const auto& target : cube_targets) {
					glTexImage2D(target, level, gl_internal_format, mip_image_dim.x, mip_image_dim.y, 0,
								 gl_format, gl_type, cube_data);
					
					// advance to next side (if not nullptr init)
					if(data != nullptr) {
						cube_data = (uint8_t*)cube_data + slice_data_size;
					}
				}
			}
			else {
				glTexImage3D(opengl_type, level, gl_internal_format,
							 mip_image_dim.x, mip_image_dim.y, int(layer_count), 0, gl_format, gl_type, level_data);
			}
		}
		else {
			switch(storage_dim_count) {
				case 1:
					glTexImage1D(opengl_type, level, gl_internal_format, mip_image_dim.x, 0, gl_format, gl_type, level_data);
					break;
				case 2:
					if(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type)) {
						glTexImage2D(opengl_type, level, gl_internal_format,
									 mip_image_dim.x, mip_image_dim.y, 0, gl_format, gl_type, level_data);
					}
					else {
						glTexImage2DMultisample(opengl_type, sample_count,
#if defined(__APPLE__)
												gl_internal_format,
#else // why is this a different type than what's in glTexImage2D?
												(GLenum)gl_internal_format,
#endif
												mip_image_dim.x, mip_image_dim.y, fixed_sample_locations);
					}
					break;
				case 3:
					if(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type)) {
						glTexImage3D(opengl_type, level, gl_internal_format,
									 mip_image_dim.x, mip_image_dim.y, mip_image_dim.z, 0, gl_format, gl_type, level_data);
					}
					else {
						glTexImage3DMultisample(opengl_type, sample_count,
#if defined(__APPLE__)
												gl_internal_format,
#else
												(GLenum)gl_internal_format,
#endif
												mip_image_dim.x, mip_image_dim.y, mip_image_dim.z, fixed_sample_locations);
					}
					break;
				default: // already checked
					floor_unreachable();
			}
		}
		
		// mip-level image data provided by user, advance pointer
		if(!generate_mip_maps && data != nullptr) {
			level_data = (uint8_t*)level_data + level_data_size;
		}
	}
	
	// generate mip-map chain if requested
	if(generate_mip_maps) {
		glGenerateMipmap(opengl_type);
	}
}

void compute_image::update_gl_image_data(const void* data) {
	if(data == nullptr) {
		log_error("data pointer must not be nullptr!");
		return;
	}
	
	const auto dim_count = image_dim_count(image_type);
	const auto storage_dim_count = image_storage_dim_count(image_type);
	const auto is_cube = has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type);
	const auto is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type);
	
	// NOTE: mip-level data always exists in "data" if this is a mip-mapped image (and automatic mip-map generation is disabled)
	const auto upload_level_count = GLint(generate_mip_maps ? 1 : mip_level_count);
	const void* level_data = data;
	int4 mip_image_dim {
		(int)image_dim.x,
		dim_count >= 2 ? (int)image_dim.y : 0,
		dim_count >= 3 ? (int)image_dim.z : 0,
		0
	};
	for(GLint level = 0; level < upload_level_count; ++level, mip_image_dim >>= 1) {
		const auto slice_data_size = image_slice_data_size_from_types(mip_image_dim, image_type, 1);
		const auto level_data_size = slice_data_size * layer_count;
		
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_BUFFER>(image_type)) {
			// TODO: how to init this?
		}
		else if(is_cube) {
			if(!is_array) {
				static constexpr const GLenum cube_targets[] {
					GL_TEXTURE_CUBE_MAP_POSITIVE_X,
					GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
					GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
					GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
					GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
					GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
				};
				
				auto cube_data = level_data;
				for(const auto& target : cube_targets) {
					glTexSubImage2D(target, level, 0, 0, mip_image_dim.x, mip_image_dim.y, gl_format, gl_type, cube_data);
					
					// advance to next side
					cube_data = (uint8_t*)cube_data + slice_data_size;
				}
			}
			else {
				// can upload cube map array directly
				glTexSubImage3D(opengl_type, level, 0, 0, 0, mip_image_dim.x, mip_image_dim.y, int(layer_count), gl_format, gl_type, level_data);
			}
		}
		else {
			switch(storage_dim_count) {
				case 1:
					glTexSubImage1D(opengl_type, level, 0, mip_image_dim.x, gl_format, gl_type, level_data);
					break;
				case 2:
					glTexSubImage2D(opengl_type, level, 0, 0, mip_image_dim.x, mip_image_dim.y, gl_format, gl_type, level_data);
					break;
				case 3:
					glTexSubImage3D(opengl_type, level, 0, 0, 0, mip_image_dim.x, mip_image_dim.y, mip_image_dim.z, gl_format, gl_type, level_data);
					break;
				default: // already checked
					floor_unreachable();
			}
		}
		
		// mip-level image data, advance pointer
		level_data = (uint8_t*)level_data + level_data_size;
	}
	
	// generate mip-map chain if requested
	if(generate_mip_maps) {
		glGenerateMipmap(opengl_type);
	}
}

compute_image::opengl_image_info compute_image::get_opengl_image_info(const uint32_t& opengl_image,
																	  const uint32_t& opengl_target,
																	  const COMPUTE_MEMORY_FLAG& flags) {
	if(opengl_image == 0) {
		if(opengl_target == GL_RENDERBUFFER) {
			log_error("the default renderbuffer can't be accessed by compute");
		}
		else log_error("invalid opengl image");
		return {};
	}
	
	opengl_image_info info;
	GLint width = 0, height = 0, depth = 0, internal_format = 0, red_type = 0, red_size = 0, depth_type = 0;
	if(glIsTexture(opengl_image)) {
		GLint cur_bound_tex = 0;
		uint32_t storage_target = opengl_target;
		switch(opengl_target) {
			case GL_TEXTURE_1D:
				info.image_type |= COMPUTE_IMAGE_TYPE::IMAGE_1D;
				glGetIntegerv(GL_TEXTURE_BINDING_1D, &cur_bound_tex);
				break;
			case GL_TEXTURE_1D_ARRAY:
				info.image_type |= COMPUTE_IMAGE_TYPE::IMAGE_1D_ARRAY;
				glGetIntegerv(GL_TEXTURE_BINDING_1D_ARRAY, &cur_bound_tex);
				break;
			case GL_TEXTURE_2D:
				info.image_type |= COMPUTE_IMAGE_TYPE::IMAGE_2D;
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &cur_bound_tex);
				break;
			case GL_TEXTURE_2D_ARRAY:
				info.image_type |= COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY;
				glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &cur_bound_tex);
				break;
			case GL_TEXTURE_2D_MULTISAMPLE:
				info.image_type |= COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA;
				glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE, &cur_bound_tex);
				break;
			case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
				info.image_type |= COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY;
				glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY, &cur_bound_tex);
				break;
			case GL_TEXTURE_3D:
				info.image_type |= COMPUTE_IMAGE_TYPE::IMAGE_3D;
				glGetIntegerv(GL_TEXTURE_BINDING_3D, &cur_bound_tex);
				break;
			case GL_TEXTURE_CUBE_MAP:
				info.image_type |= COMPUTE_IMAGE_TYPE::IMAGE_CUBE;
				glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &cur_bound_tex);
				storage_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
				break;
			case GL_TEXTURE_CUBE_MAP_ARRAY:
				info.image_type |= COMPUTE_IMAGE_TYPE::IMAGE_CUBE_ARRAY;
				glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY, &cur_bound_tex);
				break;
			case GL_TEXTURE_BUFFER:
			case GL_TEXTURE_RECTANGLE:
				log_error("GL_TEXTURE_BUFFER and GL_TEXTURE_RECTANGLE targets are currently unsupported");
				return {};
			default:
				log_error("unknown opengl texture target: %X", opengl_target);
				return {};
		}
		
		// bind query texture
		glBindTexture(opengl_target, opengl_image);
	
		glGetTexLevelParameteriv(storage_target, 0, GL_TEXTURE_WIDTH, &width);
		glGetTexLevelParameteriv(storage_target, 0, GL_TEXTURE_HEIGHT, &height);
		glGetTexLevelParameteriv(storage_target, 0, GL_TEXTURE_DEPTH, &depth);
		glGetTexLevelParameteriv(storage_target, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);
		glGetTexLevelParameteriv(storage_target, 0, GL_TEXTURE_RED_TYPE, &red_type);
		glGetTexLevelParameteriv(storage_target, 0, GL_TEXTURE_RED_SIZE, &red_size);
		glGetTexLevelParameteriv(storage_target, 0, GL_TEXTURE_DEPTH_TYPE, &depth_type);
		
		// rebind previous texture
		glBindTexture(opengl_target, (GLuint)cur_bound_tex);
	}
	else if(glIsRenderbuffer(opengl_image)) {
		info.image_type |= COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET;
		
		GLint cur_bound_rb = 0;
		glGetIntegerv(GL_RENDERBUFFER_BINDING, &cur_bound_rb);
		
		// bind query renderbuffer
		glBindRenderbuffer(GL_RENDERBUFFER, opengl_image);
		
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &height);
		
		GLint samples = 0;
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &samples);
		if(samples > 1) info.image_type |= COMPUTE_IMAGE_TYPE::FLAG_MSAA;
		
		// rebind previous renderbuffer
		glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)cur_bound_rb);
	}
	else {
		log_error("opengl object is neither a texture nor a renderbuffer: %u", opengl_image);
		return {};
	}
	info.image_dim = {
		(uint32_t)width,
		image_dim_count(info.image_type) >= 2 ? (uint32_t)height : 0,
		image_dim_count(info.image_type) >= 3 ? (uint32_t)depth : 0,
		0
	};
	
	// TODO: more formats
	static const unordered_map<int, COMPUTE_IMAGE_TYPE> format_map {
		{ GL_R8UI, COMPUTE_IMAGE_TYPE::R8UI },
		{ GL_RG8UI, COMPUTE_IMAGE_TYPE::RG8UI },
		{ GL_RGB8UI, COMPUTE_IMAGE_TYPE::RGB8UI },
		{ GL_RGBA8UI, COMPUTE_IMAGE_TYPE::RGBA8UI },
		{ GL_R8, COMPUTE_IMAGE_TYPE::R8UI_NORM },
		{ GL_RG8, COMPUTE_IMAGE_TYPE::RG8UI_NORM },
		{ GL_RGB8, COMPUTE_IMAGE_TYPE::RGB8UI_NORM },
		{ GL_RGBA8, COMPUTE_IMAGE_TYPE::RGBA8UI_NORM },
		{ GL_R8I, COMPUTE_IMAGE_TYPE::R8I },
		{ GL_RG8I, COMPUTE_IMAGE_TYPE::RG8I },
		{ GL_RGB8I, COMPUTE_IMAGE_TYPE::RGB8I },
		{ GL_RGBA8I, COMPUTE_IMAGE_TYPE::RGBA8I },
		{ GL_R8_SNORM, COMPUTE_IMAGE_TYPE::R8I_NORM },
		{ GL_RG8_SNORM, COMPUTE_IMAGE_TYPE::RG8I_NORM },
		{ GL_RGB8_SNORM, COMPUTE_IMAGE_TYPE::RGB8I_NORM },
		{ GL_RGBA8_SNORM, COMPUTE_IMAGE_TYPE::RGBA8I_NORM },
		{ GL_R16UI, COMPUTE_IMAGE_TYPE::R16UI },
		{ GL_RG16UI, COMPUTE_IMAGE_TYPE::RG16UI },
		{ GL_RGB16UI, COMPUTE_IMAGE_TYPE::RGB16UI },
		{ GL_RGBA16UI, COMPUTE_IMAGE_TYPE::RGBA16UI },
		{ GL_R16, COMPUTE_IMAGE_TYPE::R16UI_NORM },
		{ GL_RG16, COMPUTE_IMAGE_TYPE::RG16UI_NORM },
		{ GL_RGB16, COMPUTE_IMAGE_TYPE::RGB16UI_NORM },
		{ GL_RGBA16, COMPUTE_IMAGE_TYPE::RGBA16UI_NORM },
		{ GL_R16I, COMPUTE_IMAGE_TYPE::R16I },
		{ GL_RG16I, COMPUTE_IMAGE_TYPE::RG16I },
		{ GL_RGB16I, COMPUTE_IMAGE_TYPE::RGB16I },
		{ GL_RGBA16I, COMPUTE_IMAGE_TYPE::RGBA16I },
		{ GL_R16_SNORM, COMPUTE_IMAGE_TYPE::R16I_NORM },
		{ GL_RG16_SNORM, COMPUTE_IMAGE_TYPE::RG16I_NORM },
		{ GL_RGB16_SNORM, COMPUTE_IMAGE_TYPE::RGB16I_NORM },
		{ GL_RGBA16_SNORM, COMPUTE_IMAGE_TYPE::RGBA16I_NORM },
		{ GL_R32UI, COMPUTE_IMAGE_TYPE::R32UI },
		{ GL_RG32UI, COMPUTE_IMAGE_TYPE::RG32UI },
		{ GL_RGB32UI, COMPUTE_IMAGE_TYPE::RGB32UI },
		{ GL_RGBA32UI, COMPUTE_IMAGE_TYPE::RGBA32UI },
		{ GL_R32I, COMPUTE_IMAGE_TYPE::R32I },
		{ GL_RG32I, COMPUTE_IMAGE_TYPE::RG32I },
		{ GL_RGB32I, COMPUTE_IMAGE_TYPE::RGB32I },
		{ GL_RGBA32I, COMPUTE_IMAGE_TYPE::RGBA32I },
		{ GL_R16F, COMPUTE_IMAGE_TYPE::R16F },
		{ GL_RG16F, COMPUTE_IMAGE_TYPE::RG16F },
		{ GL_RGB16F, COMPUTE_IMAGE_TYPE::RGB16F },
		{ GL_RGBA16F, COMPUTE_IMAGE_TYPE::RGBA16F },
		{ GL_R32F, COMPUTE_IMAGE_TYPE::R32F },
		{ GL_RG32F, COMPUTE_IMAGE_TYPE::RG32F },
		{ GL_RGB32F, COMPUTE_IMAGE_TYPE::RGB32F },
		{ GL_RGBA32F, COMPUTE_IMAGE_TYPE::RGBA32F },
		// to handle msaa and arrays correctly, don't use the predefined aliases (msaa and/or array flag has already been set)
		{ GL_DEPTH_COMPONENT16, (COMPUTE_IMAGE_TYPE::FLAG_DEPTH | COMPUTE_IMAGE_TYPE::CHANNELS_1 |
								 COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::UINT) },
		{ GL_DEPTH_COMPONENT24, (COMPUTE_IMAGE_TYPE::FLAG_DEPTH | COMPUTE_IMAGE_TYPE::CHANNELS_1 |
								 COMPUTE_IMAGE_TYPE::FORMAT_24 | COMPUTE_IMAGE_TYPE::UINT) },
		{ GL_DEPTH_COMPONENT32, (COMPUTE_IMAGE_TYPE::FLAG_DEPTH | COMPUTE_IMAGE_TYPE::CHANNELS_1 |
								 COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::UINT) },
		{ GL_DEPTH_COMPONENT32F, (COMPUTE_IMAGE_TYPE::FLAG_DEPTH | COMPUTE_IMAGE_TYPE::CHANNELS_1 |
								  COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::FLOAT) },
		{ GL_DEPTH24_STENCIL8, (COMPUTE_IMAGE_TYPE::FLAG_DEPTH | COMPUTE_IMAGE_TYPE::CHANNELS_2 |
								COMPUTE_IMAGE_TYPE::FLAG_STENCIL | COMPUTE_IMAGE_TYPE::FORMAT_24_8 | COMPUTE_IMAGE_TYPE::UINT) },
		{ GL_DEPTH32F_STENCIL8, (COMPUTE_IMAGE_TYPE::FLAG_DEPTH | COMPUTE_IMAGE_TYPE::CHANNELS_2 |
								 COMPUTE_IMAGE_TYPE::FLAG_STENCIL | COMPUTE_IMAGE_TYPE::FORMAT_32_8 | COMPUTE_IMAGE_TYPE::FLOAT) },
	};
	const auto format_iter = format_map.find(internal_format);
	if(format_iter == cend(format_map)) {
		log_error("unknown internal format: %X", internal_format);
		return {};
	}
	info.image_type |= format_iter->second;
	
	// handle read/write flags
	switch(flags & COMPUTE_MEMORY_FLAG::READ_WRITE) {
		// if neither is set, r/w is assumed
		default:
		case COMPUTE_MEMORY_FLAG::READ_WRITE:
			info.image_type |= COMPUTE_IMAGE_TYPE::READ_WRITE;
			break;
		case COMPUTE_MEMORY_FLAG::READ:
			info.image_type |= COMPUTE_IMAGE_TYPE::READ;
			break;
		case COMPUTE_MEMORY_FLAG::WRITE:
			info.image_type |= COMPUTE_IMAGE_TYPE::WRITE;
			break;
	}
	
	// store gl types
	info.gl_internal_format = internal_format;
	
	if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(info.image_type)) {
		info.gl_type = (uint32_t)depth_type;
		info.gl_format = (has_flag<COMPUTE_IMAGE_TYPE::FLAG_STENCIL>(info.image_type) ?
						  GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT);
	}
	else {
		info.gl_type = (uint32_t)red_type;
		
		// handle special cases
		if(red_type == GL_UNSIGNED_NORMALIZED) {
			switch(red_size) {
				case 8: info.gl_type = GL_UNSIGNED_BYTE; break;
				case 16: info.gl_type = GL_UNSIGNED_SHORT; break;
				default: break; // TODO: other types
			}
		}
		else if(red_type == GL_SIGNED_NORMALIZED) {
			switch(red_size) {
				case 8: info.gl_type = GL_BYTE; break;
				case 16: info.gl_type = GL_SHORT; break;
				default: break; // TODO: other types
			}
		}
		else if(red_type == GL_FLOAT) {
			if(red_size == 16) info.gl_type = GL_HALF_FLOAT;
		}
		
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(info.image_type) ||
		   (info.image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT) {
			switch(image_channel_count(info.image_type)) {
				case 1: info.gl_format = GL_RED; break;
				case 2: info.gl_format = GL_RG; break;
				case 3: info.gl_format = GL_RGB; break;
				default:
				case 4: info.gl_format = GL_RGBA; break;
			}
		}
		else {
			switch(image_channel_count(info.image_type)) {
				case 1: info.gl_format = GL_RED_INTEGER; break;
				case 2: info.gl_format = GL_RG_INTEGER; break;
				case 3: info.gl_format = GL_RGB_INTEGER; break;
				default:
				case 4: info.gl_format = GL_RGBA_INTEGER; break;
			}
		}
	}
	
	info.valid = true;
	return info;
}

#else

compute_image::opengl_image_info compute_image::get_opengl_image_info(const uint32_t&, const uint32_t&, const COMPUTE_MEMORY_FLAG&) {
	return {};
}

#endif

uint8_t* compute_image::rgb_to_rgba(const COMPUTE_IMAGE_TYPE& rgb_type,
									const COMPUTE_IMAGE_TYPE& rgba_type,
									const uint8_t* rgb_data,
									const bool ignore_mip_levels) {
	// need to copy/convert the RGB host data to RGBA
	const auto rgba_size = image_data_size_from_types(image_dim, rgba_type, 1, ignore_mip_levels);
	const auto rgb_bytes_per_pixel = image_bytes_per_pixel(rgb_type);
	const auto rgba_bytes_per_pixel = image_bytes_per_pixel(rgba_type);
	
	uint8_t* rgba_data_ptr = new uint8_t[rgba_size];
	memset(rgba_data_ptr, 0xFF, rgba_size); // opaque
	for(size_t i = 0, count = rgba_size / rgba_bytes_per_pixel; i < count; ++i) {
		memcpy(&rgba_data_ptr[i * rgba_bytes_per_pixel],
			   &((const uint8_t*)rgb_data)[i * rgb_bytes_per_pixel],
			   rgb_bytes_per_pixel);
	}
	return rgba_data_ptr;
}

void compute_image::rgb_to_rgba_inplace(const COMPUTE_IMAGE_TYPE& rgb_type,
										const COMPUTE_IMAGE_TYPE& rgba_type,
										uint8_t* rgb_to_rgba_data,
										const bool ignore_mip_levels) {
	// need to copy/convert the RGB host data to RGBA
	const auto rgba_size = image_data_size_from_types(image_dim, rgba_type, 1, ignore_mip_levels);
	const auto rgb_bytes_per_pixel = image_bytes_per_pixel(rgb_type);
	const auto rgba_bytes_per_pixel = image_bytes_per_pixel(rgba_type);
	const auto alpha_size = rgba_bytes_per_pixel / 4;
	
	// this needs to happen in reverse, otherwise we'd be overwriting the following RGB data
	for(size_t count = rgba_size / rgba_bytes_per_pixel, i = count - 1; ; --i) {
		for(size_t j = 0; j < rgb_bytes_per_pixel; ++j) {
			rgb_to_rgba_data[i * rgba_bytes_per_pixel + j] = rgb_to_rgba_data[i * rgb_bytes_per_pixel + j];
		}
		memset(&rgb_to_rgba_data[(i + 1) * rgba_bytes_per_pixel] - alpha_size, 0xFF, alpha_size); // opaque
		
		if(i == 0) break;
	}
}

uint8_t* compute_image::rgba_to_rgb(const COMPUTE_IMAGE_TYPE& rgba_type,
									const COMPUTE_IMAGE_TYPE& rgb_type,
									const uint8_t* rgba_data,
									uint8_t* dst_rgb_data,
									const bool ignore_mip_levels) {
	// need to copy/convert the RGB host data to RGBA
	const auto rgba_size = image_data_size_from_types(image_dim, rgba_type, 1, ignore_mip_levels);
	const auto rgb_size = image_data_size_from_types(image_dim, rgb_type, 1, ignore_mip_levels);
	const auto rgb_bytes_per_pixel = image_bytes_per_pixel(rgb_type);
	const auto rgba_bytes_per_pixel = image_bytes_per_pixel(rgba_type);
	
	uint8_t* rgb_data_ptr = (dst_rgb_data != nullptr ? dst_rgb_data : new uint8_t[rgb_size]);
	for(size_t i = 0, count = rgba_size / rgba_bytes_per_pixel; i < count; ++i) {
		memcpy(&rgb_data_ptr[i * rgb_bytes_per_pixel],
			   &((const uint8_t*)rgba_data)[i * rgba_bytes_per_pixel],
			   rgb_bytes_per_pixel);
	}
	return (dst_rgb_data != nullptr ? nullptr : rgb_data_ptr);
}

// something about dog food
#if !defined(FLOOR_NO_HOST_COMPUTE)
#include <floor/compute/device/common.hpp>
#define FLOOR_COMPUTE_HOST_MINIFY 1 // needed now so that kernel code will actually be included
#else // when not using host-compute, set these two defines so depth images are actually supported on other backends
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_SUPPORT_1
#define FLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_WRITE_SUPPORT_1
#endif
#include <floor/compute/device/mip_map_minify.hpp>

void compute_image::build_mip_map_minification_program() const {
	// build mip-map minify kernels (do so in a separate thread so that we don't hold up anything)
	task::spawn([this, ctx = dev->context]() {
		auto prog = make_unique<minify_program>();
		const llvm_toolchain::compile_options options {
			// suppress any debug output for this, we only want to have console/log output if something goes wrong
			.silence_debug_output = true
		};
		
		string base_path = "";
		switch(ctx->get_compute_type()) {
			case COMPUTE_TYPE::CUDA:
				base_path = floor::get_cuda_base_path();
				break;
			case COMPUTE_TYPE::OPENCL:
				base_path = floor::get_opencl_base_path();
				break;
			case COMPUTE_TYPE::VULKAN:
				base_path = floor::get_vulkan_base_path();
				break;
			case COMPUTE_TYPE::HOST:
				// doesn't matter
				break;
			default:
				log_error("backend does not support (or need) mip-map minification kernels");
				return;
		}
		
		prog->program = ctx->add_program_file(base_path + "floor/floor/compute/device/mip_map_minify.hpp", options);
		if(prog->program == nullptr) {
			log_error("failed to build minify kernels");
			return;
		}
		
		// build/get all minification kernels
		unordered_map<COMPUTE_IMAGE_TYPE, pair<string, shared_ptr<compute_kernel>>> minify_kernels {
#define FLOOR_MINIFY_ENTRY(image_type, sample_type) \
			{ \
				COMPUTE_IMAGE_TYPE::image_type | COMPUTE_IMAGE_TYPE::sample_type, \
				{ "libfloor_mip_map_minify_" #image_type "_" #sample_type , {} } \
			},
		
			FLOOR_MINIFY_IMAGE_TYPES(FLOOR_MINIFY_ENTRY)
		};
		
		// drop depth image kernel (handling) if there is no depth image support
		if(!dev->image_depth_support ||
		   !dev->image_depth_write_support ||
		   // TODO: vulkan support
		   ctx->get_compute_type() == COMPUTE_TYPE::VULKAN) {
			minify_kernels.erase(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH | COMPUTE_IMAGE_TYPE::FLOAT);
			minify_kernels.erase(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_ARRAY | COMPUTE_IMAGE_TYPE::FLOAT);
		}
		
		for(auto& entry : minify_kernels) {
			entry.second.second = prog->program->get_kernel(entry.second.first);
			if(entry.second.second == nullptr) {
				log_error("failed to retrieve kernel \"%s\" from minify program", entry.second.first);
				return;
			}
		}
		minify_kernels.swap(prog->kernels);
		
		// done, set programs for this context and return
		GUARD(minify_programs_mtx);
		minify_programs[ctx] = move(prog);
		
		// TODO: safely/cleanly hold up compute_context destruction if the program exits before this is built
	}, "minify build");
}

void compute_image::generate_mip_map_chain(shared_ptr<compute_queue> cqueue) const {
	if(cqueue == nullptr) return;
	
	for(uint32_t try_out = 0; ; ++try_out) {
		if(try_out == 100) {
			log_error("mip-map minify kernel compilation is stuck?");
			return;
		}
		if(try_out > 0) {
			this_thread::sleep_for(50ms);
		}
		
		// get the compiled program for this context
		minify_programs_mtx.lock();
		const auto iter = minify_programs.find(dev->context);
		if(iter == minify_programs.end()) {
			// kick off build + insert nullptr value to signal build has started
			build_mip_map_minification_program();
			minify_programs.emplace(dev->context, nullptr);
			
			minify_programs_mtx.unlock();
			continue;
		}
		if(iter->second == nullptr) {
			// still building
			minify_programs_mtx.unlock();
			continue;
		}
		const auto prog = iter->second.get();
		minify_programs_mtx.unlock();
		
		// find the appropriate kernel for this image type
		const auto image_base_type = minify_image_base_type(image_type);
		const auto kernel_iter = prog->kernels.find(image_base_type);
		if(kernel_iter == prog->kernels.end()) {
			log_error("no minification kernel for this image type exists: %X", image_type);
			return;
		}
		auto minify_kernel = kernel_iter->second.second;
		
		// run the kernel for this image
		const auto dim_count = image_dim_count(image_type);
		uint3 lsize;
		switch(dim_count) {
			case 1: lsize = { dev->max_total_local_size, 1, 1 }; break;
			case 2: lsize = { (dev->max_total_local_size > 256 ? 32 : 16), (dev->max_total_local_size > 512 ? 32 : 16), 1 }; break;
			default:
			case 3: lsize = { (dev->max_total_local_size > 512 ? 32 : 16), (dev->max_total_local_size > 256 ? 16 : 8), 2 }; break;
		}
		// TODO: vulkan 2D kernel execution
		if(dev->context->get_compute_type() == COMPUTE_TYPE::VULKAN) {
			lsize = { dev->max_total_local_size, 1, 1 };
		}
		for(uint32_t layer = 0; layer < layer_count; ++layer) {
			uint3 level_size {
				image_dim.x,
				dim_count >= 2 ? image_dim.y : 0u,
				dim_count >= 3 ? image_dim.z : 0u,
			};
			float3 inv_prev_level_size;
			for(uint32_t level = 0; level < mip_level_count;
				++level, inv_prev_level_size = 1.0f / float3(level_size), level_size >>= 1) {
				if(level == 0) continue;
				cqueue->execute(minify_kernel, level_size.rounded_next_multiple(lsize), lsize,
								(const compute_image*)this, level_size, inv_prev_level_size, level, layer);
			}
		}
		
		// done
		return;
	}
}

string compute_image::image_type_to_string(const COMPUTE_IMAGE_TYPE& type) {
	stringstream ret;
	
	// base type
	const auto dim = image_dim_count(type);
	const auto is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(type);
	const auto is_depth = has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type);
	const auto is_stencil = has_flag<COMPUTE_IMAGE_TYPE::FLAG_STENCIL>(type);
	const auto is_msaa = has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(type);
	const auto is_cube = has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(type);
	const auto is_buffer = has_flag<COMPUTE_IMAGE_TYPE::FLAG_BUFFER>(type);
	
	if(!is_cube) ret << dim << "D ";
	else ret << "Cube ";
	
	if(is_depth) ret << "Depth ";
	if(is_stencil) ret << "Stencil ";
	if(is_msaa) ret << "MSAA ";
	if(is_array) ret << "Array ";
	if(is_buffer) ret << "Buffer ";
	
	// channel count and layout
	const auto channel_count = image_channel_count(type);
	const auto layout_idx = (uint32_t(type & COMPUTE_IMAGE_TYPE::__LAYOUT_MASK) >> uint32_t(COMPUTE_IMAGE_TYPE::__LAYOUT_SHIFT));
	static const char layout_strs[4][5] {
		"RGBA",
		"BGRA",
		"ABGR",
		"ARGB",
	};
	for(uint32_t i = 0; i < channel_count; ++i) {
		ret << layout_strs[layout_idx][i];
	}
	ret << " ";
	if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_SRGB>(type)) ret << "sRGB ";
	
	// data type
	if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(type)) ret << "normalized ";
	switch(type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) {
		case COMPUTE_IMAGE_TYPE::INT: ret << "int "; break;
		case COMPUTE_IMAGE_TYPE::UINT: ret << "uint "; break;
		case COMPUTE_IMAGE_TYPE::FLOAT: ret << "float "; break;
		default: ret << "INVALID-DATA-TYPE "; break;
	}
	
	// access
	if(has_flag<COMPUTE_IMAGE_TYPE::READ_WRITE>(type)) ret << "read/write ";
	else if(has_flag<COMPUTE_IMAGE_TYPE::READ>(type)) ret << "read-only ";
	else if(has_flag<COMPUTE_IMAGE_TYPE::WRITE>(type)) ret << "write-only ";
	
	// compression
	if(image_compressed(type)) {
		switch(type & COMPUTE_IMAGE_TYPE::__COMPRESSION_MASK) {
			case COMPUTE_IMAGE_TYPE::BC1: ret << "BC1 "; break;
			case COMPUTE_IMAGE_TYPE::BC2: ret << "BC2 "; break;
			case COMPUTE_IMAGE_TYPE::BC3: ret << "BC3 "; break;
			case COMPUTE_IMAGE_TYPE::RGTC: ret << "RGTC/BC4/BC5 "; break;
			case COMPUTE_IMAGE_TYPE::BPTC: ret << "BPTC/BC6/BC7 "; break;
			case COMPUTE_IMAGE_TYPE::PVRTC: ret << "PVRTC "; break;
			case COMPUTE_IMAGE_TYPE::PVRTC2: ret << "PVRTC2 "; break;
			case COMPUTE_IMAGE_TYPE::EAC: ret << "EAC/ETC1 "; break;
			case COMPUTE_IMAGE_TYPE::ETC2: ret << "ETC2 "; break;
			case COMPUTE_IMAGE_TYPE::ASTC: ret << "ASTC "; break;
			default: ret << "INVALID-COMPRESSION-TYPE "; break;
		}
	}
	
	// format
	switch(type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) {
		case COMPUTE_IMAGE_TYPE::FORMAT_1: ret << "1bpc"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_2: ret << "2bpc"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_3_3_2: ret << "3/3/2"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_4: ret << "4bpc"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_4_2_0: ret << "4/2/0"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_4_1_1: ret << "4/1/1"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_4_2_2: ret << "4/2/2"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5: ret << "5/5/5"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5_ALPHA_1: ret << "5/5/5/A1"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_5_6_5: ret << "5/6/5"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_8: ret << "8bpc"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_9_9_9_EXP_5: ret << "9/9/9/E5"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_10: ret << "10/10/10"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_10_10_10_ALPHA_2: ret << "10/10/10/A2"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_11_11_10: ret << "11/11/10"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12: ret << "12/12/12"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12_12: ret << "12/12/12/12"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_16: ret << "16bpc"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_16_8: ret << "16/8"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_24: ret << "24"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_24_8: ret << "24/8"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_32: ret << "32bpc"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_32_8: ret << "32/8"; break;
		case COMPUTE_IMAGE_TYPE::FORMAT_64: ret << "64bpc"; break;
		default: ret << "INVALID-FORMAT"; break;
	}
	
	// other
	if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED>(type)) ret << " mip-mapped";
	if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET>(type)) ret << " render-target";
	
	return ret.str();
}

void compute_image::set_shim_type_info() {
	// set shim format to the corresponding 4-channel format
	// compressed images will always be used in their original state, even if they are RGB
	if(image_channel_count(image_type) == 3 && !image_compressed(image_type)) {
		shim_image_type = (image_type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK) | COMPUTE_IMAGE_TYPE::RGBA;
		shim_image_data_size = image_data_size_from_types(image_dim, shim_image_type, 1, generate_mip_maps);
		shim_image_data_size_mip_maps = image_data_size_from_types(image_dim, shim_image_type, 1, false);
	}
	// == original type if not 3-channel -> 4-channel emulation
	else shim_image_type = image_type;
}
