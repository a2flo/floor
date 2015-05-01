/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#include <floor/compute/compute_image.hpp>
#include <floor/core/logger.hpp>

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
	
	glTexParameteri(opengl_type, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(opengl_type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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
	const int4 gl_dim { image_dim };
	const GLsizei sample_count { 4 }; // TODO: support this properly
	const GLint level { 0 }; // TODO: support this properly
	const bool fixed_sample_locations { false }; // TODO: support this properly
	
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
					case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5_1:
						type = GL_UNSIGNED_SHORT_5_5_5_1;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_5_6_5:
						type = GL_UNSIGNED_SHORT_5_6_5;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_10:
						type = GL_UNSIGNED_INT;
						break;
					case COMPUTE_IMAGE_TYPE::FORMAT_10_10_10_2:
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
					case COMPUTE_IMAGE_TYPE::FORMAT_9_9_9_5:
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
					case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5_1:
					case COMPUTE_IMAGE_TYPE::FORMAT_5_6_5:
					case COMPUTE_IMAGE_TYPE::FORMAT_9_9_9_5:
					case COMPUTE_IMAGE_TYPE::FORMAT_10:
					case COMPUTE_IMAGE_TYPE::FORMAT_10_10_10_2:
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
					case COMPUTE_IMAGE_TYPE::FORMAT_9_9_9_5:
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
					case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5_1:
					case COMPUTE_IMAGE_TYPE::FORMAT_5_6_5:
					case COMPUTE_IMAGE_TYPE::FORMAT_10:
					case COMPUTE_IMAGE_TYPE::FORMAT_10_10_10_2:
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
	
	if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_BUFFER>(image_type)) {
		// TODO: how to init this?
	}
	else if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type)) {
		const auto size_per_side = image_data_size / 6;
		
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, level, internal_format, gl_dim.x, gl_dim.y, 0,
					 format, type, pixel_ptr);
		pixel_ptr = (void*)((uint8_t*)pixel_ptr + size_per_side);
		
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, level, internal_format, gl_dim.x, gl_dim.y, 0,
					 format, type, pixel_ptr);
		pixel_ptr = (void*)((uint8_t*)pixel_ptr + size_per_side);
		
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, level, internal_format, gl_dim.x, gl_dim.y, 0,
					 format, type, pixel_ptr);
		pixel_ptr = (void*)((uint8_t*)pixel_ptr + size_per_side);
		
		glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, level, internal_format, gl_dim.x, gl_dim.y, 0,
					 format, type, pixel_ptr);
		pixel_ptr = (void*)((uint8_t*)pixel_ptr + size_per_side);
		
		glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, level, internal_format, gl_dim.x, gl_dim.y, 0,
					 format, type, pixel_ptr);
		pixel_ptr = (void*)((uint8_t*)pixel_ptr + size_per_side);
		
		glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, level, internal_format, gl_dim.x, gl_dim.y, 0,
					 format, type, pixel_ptr);
	}
	else {
		switch(image_type & COMPUTE_IMAGE_TYPE::__DIM_STORAGE_MASK) {
			case COMPUTE_IMAGE_TYPE::DIM_STORAGE_1D:
				glTexImage1D(opengl_type, level, internal_format, gl_dim.x, 0, format, type, pixel_ptr);
				break;
			case COMPUTE_IMAGE_TYPE::DIM_STORAGE_2D:
				if(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type)) {
					glTexImage2D(opengl_type, level, internal_format,
								 gl_dim.x, gl_dim.y, 0, format, type, pixel_ptr);
				}
				else {
					glTexImage2DMultisample(opengl_type, sample_count,
#if defined(__APPLE__)
											internal_format,
#else // why is this a different type than what's in glTexImage2D?
											(GLenum)internal_format,
#endif
											gl_dim.x, gl_dim.y, fixed_sample_locations);
				}
				break;
			case COMPUTE_IMAGE_TYPE::DIM_STORAGE_3D:
				if(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type)) {
					glTexImage3D(opengl_type, level, internal_format,
								 gl_dim.x, gl_dim.y, gl_dim.z, 0, format, type, pixel_ptr);
				}
				else {
					glTexImage3DMultisample(opengl_type, sample_count,
#if defined(__APPLE__)
											internal_format,
#else
											(GLenum)internal_format,
#endif
											gl_dim.x, gl_dim.y, gl_dim.z, fixed_sample_locations);
				}
				break;
			default:
				log_error("storage dimension not set!");
				break;
		}
	}
	
	// if everything has been successful, store the used gl types for later use
	gl_internal_format = internal_format;
	gl_format = format;
	gl_type = type;
	
	return true;
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
	GLint width = 0, height = 0, depth = 0, internal_format = 0, red_type = 0, depth_type = 0;
	if(glIsTexture(opengl_image)) {
		GLint cur_bound_tex = 0;
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
	
		glGetTexLevelParameteriv(opengl_target, 0, GL_TEXTURE_WIDTH, &width);
		glGetTexLevelParameteriv(opengl_target, 0, GL_TEXTURE_HEIGHT, &height);
		glGetTexLevelParameteriv(opengl_target, 0, GL_TEXTURE_DEPTH, &depth);
		glGetTexLevelParameteriv(opengl_target, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);
		glGetTexLevelParameteriv(opengl_target, 0, GL_TEXTURE_RED_TYPE, &red_type);
		glGetTexLevelParameteriv(opengl_target, 0, GL_TEXTURE_DEPTH_TYPE, &depth_type);
		
		// rebind previous texture
		glBindTexture(opengl_target, (GLuint)cur_bound_tex);
	}
	else if(glIsRenderbuffer(opengl_image)) {
		info.image_type |= COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::FLAG_RENDERBUFFER;
		
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
		{ GL_R8, COMPUTE_IMAGE_TYPE::R8UI },
		{ GL_RG8, COMPUTE_IMAGE_TYPE::RG8UI },
		{ GL_RGB8, COMPUTE_IMAGE_TYPE::RGB8UI },
		{ GL_RGBA8, COMPUTE_IMAGE_TYPE::RGBA8UI },
		{ GL_R8UI, COMPUTE_IMAGE_TYPE::R8UI },
		{ GL_RG8UI, COMPUTE_IMAGE_TYPE::RG8UI },
		{ GL_RGB8UI, COMPUTE_IMAGE_TYPE::RGB8UI },
		{ GL_RGBA8UI, COMPUTE_IMAGE_TYPE::RGBA8UI },
		{ GL_R8I, COMPUTE_IMAGE_TYPE::R8I },
		{ GL_RG8I, COMPUTE_IMAGE_TYPE::RG8I },
		{ GL_RGB8I, COMPUTE_IMAGE_TYPE::RGB8I },
		{ GL_RGBA8I, COMPUTE_IMAGE_TYPE::RGBA8I },
		{ GL_R8_SNORM, COMPUTE_IMAGE_TYPE::R8I },
		{ GL_RG8_SNORM, COMPUTE_IMAGE_TYPE::RG8I },
		{ GL_RGB8_SNORM, COMPUTE_IMAGE_TYPE::RGB8I },
		{ GL_RGBA8_SNORM, COMPUTE_IMAGE_TYPE::RGBA8I },
		{ GL_R16, COMPUTE_IMAGE_TYPE::R16UI },
		{ GL_RG16, COMPUTE_IMAGE_TYPE::RG16UI },
		{ GL_RGB16, COMPUTE_IMAGE_TYPE::RGB16UI },
		{ GL_RGBA16, COMPUTE_IMAGE_TYPE::RGBA16UI },
		{ GL_R16UI, COMPUTE_IMAGE_TYPE::R16UI },
		{ GL_RG16UI, COMPUTE_IMAGE_TYPE::RG16UI },
		{ GL_RGB16UI, COMPUTE_IMAGE_TYPE::RGB16UI },
		{ GL_RGBA16UI, COMPUTE_IMAGE_TYPE::RGBA16UI },
		{ GL_R16I, COMPUTE_IMAGE_TYPE::R16I },
		{ GL_RG16I, COMPUTE_IMAGE_TYPE::RG16I },
		{ GL_RGB16I, COMPUTE_IMAGE_TYPE::RGB16I },
		{ GL_RGBA16I, COMPUTE_IMAGE_TYPE::RGBA16I },
		{ GL_R16_SNORM, COMPUTE_IMAGE_TYPE::R16I },
		{ GL_RG16_SNORM, COMPUTE_IMAGE_TYPE::RG16I },
		{ GL_RGB16_SNORM, COMPUTE_IMAGE_TYPE::RGB16I },
		{ GL_RGBA16_SNORM, COMPUTE_IMAGE_TYPE::RGBA16I },
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
		
		switch(image_channel_count(info.image_type)) {
			case 1: info.gl_format = GL_RED; break;
			case 2: info.gl_format = GL_RG; break;
			case 3: info.gl_format = GL_RGB; break;
			default:
			case 4: info.gl_format = GL_RGBA; break;
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
