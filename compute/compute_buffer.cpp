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

#include <floor/compute/compute_buffer.hpp>
#include <floor/core/logger.hpp>

enum_class_bitwise_and_global_impl(COMPUTE_BUFFER_FLAG)
enum_class_bitwise_or_global_impl(COMPUTE_BUFFER_FLAG)
enum_class_bitwise_and_global_impl(COMPUTE_BUFFER_MAP_FLAG)
enum_class_bitwise_or_global_impl(COMPUTE_BUFFER_MAP_FLAG)

compute_buffer::compute_buffer(const void* ctx_ptr_,
							   const size_t& size_,
							   void* host_ptr_,
							   const COMPUTE_BUFFER_FLAG flags_) :
ctx_ptr(ctx_ptr_), size(align_size(size_)), host_ptr(host_ptr_), flags(flags_) {
	if(size == 0) {
		log_error("can't allocate a buffer of size 0!");
	}
	else if(size_ != size) {
		log_error("buffer size must always be a multiple of %u! - using size of %u instead of %u now",
				  min_multiple(), size, size_);
	}
}

compute_buffer::~compute_buffer() {}
