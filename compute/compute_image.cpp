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

static constexpr size_t image_data_size_from_types(const uint32_t& channel_count,
												   const uint4& image_dim,
												   const COMPUTE_IMAGE_TYPE& image_type,
												   const COMPUTE_IMAGE_STORAGE_TYPE& storage_type) {
	const auto dim_count = compute_image::dim_count(image_type);
	size_t size = size_t(channel_count) * size_t(image_dim.x);
	if(dim_count >= 2) size *= size_t(image_dim.y);
	if(dim_count >= 3) size *= size_t(image_dim.z);
	if(dim_count == 4) size *= size_t(image_dim.w);
	
	if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type)) {
		// array count after: width (* height (* depth))
		size *= size_t(image_dim[dim_count]);
	}
	
	if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type)) {
		// 6 cube sides
		size *= 6u;
	}
	
	// TODO: make sure special formats correspond to channel count
	switch(storage_type) {
		case COMPUTE_IMAGE_STORAGE_TYPE::INT_8:
		case COMPUTE_IMAGE_STORAGE_TYPE::UINT_8:
		case COMPUTE_IMAGE_STORAGE_TYPE::NORM_INT_8:
		case COMPUTE_IMAGE_STORAGE_TYPE::NORM_UINT_8:
			// * 1
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::INT_16:
		case COMPUTE_IMAGE_STORAGE_TYPE::UINT_16:
		case COMPUTE_IMAGE_STORAGE_TYPE::FLOAT_16:
		case COMPUTE_IMAGE_STORAGE_TYPE::NORM_INT_16:
		case COMPUTE_IMAGE_STORAGE_TYPE::NORM_UINT_16:
			size *= 2;
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::INT_24:
		case COMPUTE_IMAGE_STORAGE_TYPE::UINT_24:
		case COMPUTE_IMAGE_STORAGE_TYPE::NORM_INT_24:
		case COMPUTE_IMAGE_STORAGE_TYPE::NORM_UINT_24:
			size *= 3;
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::INT_32:
		case COMPUTE_IMAGE_STORAGE_TYPE::UINT_32:
		case COMPUTE_IMAGE_STORAGE_TYPE::FLOAT_32:
		case COMPUTE_IMAGE_STORAGE_TYPE::NORM_INT_32:
		case COMPUTE_IMAGE_STORAGE_TYPE::NORM_UINT_32:
			size *= 4;
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::INT_64:
		case COMPUTE_IMAGE_STORAGE_TYPE::UINT_64:
		case COMPUTE_IMAGE_STORAGE_TYPE::FLOAT_64:
			size *= 8;
			break;
		// special cases
		case COMPUTE_IMAGE_STORAGE_TYPE::INT_565:
		case COMPUTE_IMAGE_STORAGE_TYPE::UINT_565:
			size *= 2; // 16-bit format with 3 channels
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::INT_5:
		case COMPUTE_IMAGE_STORAGE_TYPE::UINT_5:
			size *= 2; // 3 channels / 15-bit, or 4 channels / 16-bit
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::INT_10:
		case COMPUTE_IMAGE_STORAGE_TYPE::UINT_10:
			size *= 4; // 3 channels / 30-bit, or 4 channels / 32-bit
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::NONE:
		case COMPUTE_IMAGE_STORAGE_TYPE::__COMPUTE_IMAGE_STORAGE_TYPE_MAX:
			floor_unreachable();
	}
	
	return size;
}

compute_image::compute_image(const void* device,
							 const uint4 image_dim_,
							 const COMPUTE_IMAGE_TYPE image_type_,
							 const COMPUTE_IMAGE_STORAGE_TYPE storage_type_,
							 const uint32_t channel_count_,
							 void* host_ptr_,
							 const COMPUTE_MEMORY_FLAG flags_,
							 const uint32_t opengl_type_) :
compute_memory(device, host_ptr_, flags_, opengl_type_),
image_dim(image_dim_), image_type(image_type_), storage_type(storage_type_), channel_count(channel_count_),
image_data_size(image_data_size_from_types(channel_count, image_dim, image_type, storage_type)) {
	// TODO: make sure format is supported, fail early if not
	// TODO: if opengl_type is 0 and opengl sharing is enabled, try guessing it, otherwise fail
}

compute_image::~compute_image() {}

void compute_image::delete_gl_image() {
	if(gl_object == 0) return;
	glDeleteTextures(1, &gl_object);
	gl_object = 0;
}

bool compute_image::create_gl_image(const bool copy_host_data) {
	// clean up previous gl image that might still exist
	delete_gl_image();
	
	// create and init the opengl image
	glGenTextures(1, &gl_object);
	glBindTexture(opengl_type, gl_object);
	if(gl_object == 0 || !glIsTexture(gl_object)) {
		log_error("created opengl image %u is invalid!", gl_object);
		return false;
	}
	
	glTexParameteri(opengl_type, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(opengl_type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(opengl_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	if(storage_dim_count(image_type) >= 2) {
		glTexParameteri(opengl_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	if(storage_dim_count(image_type) >= 3) {
		glTexParameteri(opengl_type, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	}
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
		// TODO: handle other formats
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_STENCIL>(image_type)) {
			internal_format = GL_DEPTH_STENCIL;
			format = GL_DEPTH_STENCIL;
		}
		else {
			internal_format = GL_DEPTH_COMPONENT;
			format = GL_DEPTH_COMPONENT;
		}
	}
	else {
		if(channel_count == 1) internal_format = GL_RED;
		else if(channel_count == 2) internal_format = GL_RG;
		else if(channel_count == 3) internal_format = GL_RGB;
		else internal_format = GL_RGBA;
		format = (GLenum)internal_format;
	}
	
	switch(storage_type) {
		case COMPUTE_IMAGE_STORAGE_TYPE::INT_8:
		case COMPUTE_IMAGE_STORAGE_TYPE::NORM_INT_8:
			type = GL_BYTE;
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::UINT_8:
		case COMPUTE_IMAGE_STORAGE_TYPE::NORM_UINT_8:
			type = GL_UNSIGNED_BYTE;
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::INT_16:
		case COMPUTE_IMAGE_STORAGE_TYPE::NORM_INT_16:
			type = GL_SHORT;
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::UINT_16:
		case COMPUTE_IMAGE_STORAGE_TYPE::NORM_UINT_16:
			type = GL_UNSIGNED_SHORT;
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::INT_24:
		case COMPUTE_IMAGE_STORAGE_TYPE::NORM_INT_24:
			type = GL_UNSIGNED_INT; // TODO: is this correct?
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::UINT_24:
		case COMPUTE_IMAGE_STORAGE_TYPE::NORM_UINT_24:
			type = GL_UNSIGNED_INT; // TODO: is this correct?
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::INT_32:
		case COMPUTE_IMAGE_STORAGE_TYPE::NORM_INT_32:
			type = GL_INT;
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::UINT_32:
		case COMPUTE_IMAGE_STORAGE_TYPE::NORM_UINT_32:
			type = GL_UNSIGNED_INT;
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::FLOAT_16:
		case COMPUTE_IMAGE_STORAGE_TYPE::FLOAT_32:
			type = GL_FLOAT;
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::INT_64:
		case COMPUTE_IMAGE_STORAGE_TYPE::UINT_64:
		case COMPUTE_IMAGE_STORAGE_TYPE::FLOAT_64:
			// TODO: unsupported
			break;
		// special cases
		case COMPUTE_IMAGE_STORAGE_TYPE::INT_565:
			// TODO: unsupported
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::UINT_565:
			type = GL_UNSIGNED_SHORT_5_6_5;
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::INT_5:
			// TODO: unsupported
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::UINT_5:
			type = GL_UNSIGNED_SHORT_5_5_5_1;
			// TODO: channel count == 4, make sure
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::INT_10:
			// TODO: unsupported
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::UINT_10:
			type = GL_UNSIGNED_INT_10_10_10_2;
			// TODO: channel count == 4, make sure
			break;
		case COMPUTE_IMAGE_STORAGE_TYPE::NONE:
		case COMPUTE_IMAGE_STORAGE_TYPE::__COMPUTE_IMAGE_STORAGE_TYPE_MAX:
			floor_unreachable();
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
		pixel_ptr = (void*)((uint8_t*)pixel_ptr + size_per_side);
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
					glTexImage2DMultisample(opengl_type, sample_count, internal_format,
											gl_dim.x, gl_dim.y, fixed_sample_locations);
				}
				break;
			case COMPUTE_IMAGE_TYPE::DIM_STORAGE_3D:
				if(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type)) {
					glTexImage3D(opengl_type, level, internal_format,
								 gl_dim.x, gl_dim.y, gl_dim.z, 0, format, type, pixel_ptr);
				}
				else {
					glTexImage3DMultisample(opengl_type, sample_count, internal_format,
											gl_dim.x, gl_dim.y, gl_dim.z, fixed_sample_locations);
				}
				break;
			default:
			case COMPUTE_IMAGE_TYPE::DIM_STORAGE_4D: // not sure if anything actually uses this?
				log_error("storage format is not supported!");
				break;
		}
	}
	
	return true;
}
