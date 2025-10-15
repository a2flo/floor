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

#include <floor/device/metal/metal_buffer.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/core/logger.hpp>
#include <floor/device/metal/metal_queue.hpp>
#include <floor/device/metal/metal_device.hpp>
#include <floor/device/metal/metal_context.hpp>
#include <floor/darwin/darwin_helper.hpp>
#include <floor/floor.hpp>

namespace fl {

// TODO: proper error (return) value handling everywhere

static MEMORY_FLAG infer_metal_buffer_flags(MEMORY_FLAG flags, const metal_device& mtl_dev) {
	if (mtl_dev.unified_memory) {
		if (// always use shared memory?
			floor::get_metal_shared_only_with_unified_memory() ||
			// will already use shared memory if either HOST_READ or HOST_WRITE is set
			(flags & MEMORY_FLAG::HOST_READ_WRITE) != MEMORY_FLAG::NONE) {
			// any shared memory combination -> can always use HOST_READ_WRITE
			flags |= MEMORY_FLAG::HOST_READ_WRITE;
		}
	}
	return flags;
}

metal_buffer::metal_buffer(const bool is_staging_buffer_,
						   const device_queue& cqueue,
						   const size_t& size_,
						   std::span<uint8_t> host_data_,
						   const MEMORY_FLAG flags_) :
device_buffer(cqueue, size_, host_data_, infer_metal_buffer_flags(flags_, (const metal_device&)cqueue.get_device())),
is_staging_buffer(is_staging_buffer_) {
	if(size < min_multiple()) return;
	
	// no special MEMORY_FLAG::READ_WRITE handling for metal, buffers are always read/write
	
	const auto shared_only = (floor::get_metal_shared_only_with_unified_memory() && dev.unified_memory);
	switch (flags & MEMORY_FLAG::HOST_READ_WRITE) {
		case MEMORY_FLAG::HOST_READ:
		case MEMORY_FLAG::HOST_READ_WRITE:
			// keep the default MTLCPUCacheModeDefaultCache
			break;
		case MEMORY_FLAG::NONE:
		case MEMORY_FLAG::HOST_WRITE:
			// host will only write or not read/write at all -> can use write combined
			if (!shared_only) {
				options = MTLCPUCacheModeWriteCombined;
			}
			break;
		// all possible cases handled
		default: floor_unreachable();
	}
	
	if (!shared_only && (flags & MEMORY_FLAG::HOST_READ_WRITE) == MEMORY_FLAG::NONE) {
		// if buffer is not accessed by the host at all, use private storage
		// note that this disables pretty much all functionality of this class!
		options |= MTLResourceStorageModePrivate;
	} else {
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		if (!dev.unified_memory) {
			if (!is_staging_buffer && !has_flag<MEMORY_FLAG::USE_HOST_MEMORY>(flags)) {
				// for performance reasons, still use private storage here, but also create a host-accessible staging buffer
				// that we'll use to copy memory to and from the private storage buffer
				options |= MTLResourceStorageModePrivate;
				staging_buffer = std::make_unique<metal_buffer>(true, cqueue, size, std::span<uint8_t> {},
														   MEMORY_FLAG::READ_WRITE |
														   (flags & MEMORY_FLAG::HOST_READ_WRITE));
				staging_buffer->set_debug_label(debug_label + "_staging_buffer");
			} else {
				// use managed storage for the staging buffer or host memory backed buffer
				// note that this requires us to perform explicit sync operations
				options |= MTLResourceStorageModeManaged;
			}
		} else {
			// used shared storage when we have unified memory that needs to be accessed by both the CPU and GPU
			options |= MTLResourceStorageModeShared;
		}
#else
		// iOS only knows private and shared storage modes
		options |= MTLResourceStorageModeShared;
#endif
	}
	
	if (has_flag<MEMORY_FLAG::NO_RESOURCE_TRACKING>(flags) ||
		// "flags" may be specified externally -> also check context flag here
		has_flag<DEVICE_CONTEXT_FLAGS::NO_RESOURCE_TRACKING>(dev.context->get_context_flags())) {
		options |= MTLResourceHazardTrackingModeUntracked;
	}
	
	// both heap flags must be enabled for this to be viable + must obviously not be backed by CPU memory
	if (!has_flag<MEMORY_FLAG::USE_HOST_MEMORY>(flags) &&
		should_heap_allocate_device_memory(dev.context->get_context_flags(), flags)) {
		// must also be untracked and in private or shared storage mode
		if ((options & MTLResourceHazardTrackingModeMask) == MTLResourceHazardTrackingModeUntracked &&
			((options & MTLResourceStorageModeMask) == MTLResourceStorageModePrivate ||
			 (options & MTLResourceStorageModeMask) == MTLResourceStorageModeShared)) {
			// always use default
			options &= ~MTLResourceCPUCacheModeMask;
			options |= MTLCPUCacheModeDefaultCache;
			// enable (for now), may get disabled in create_internal() if other conditions aren't met
			is_heap_buffer = true;
		}
	}
	
	// TODO: handle the remaining flags + host ptr
	
	// actually create the buffer
	if (!create_internal(true, cqueue)) {
		return; // can't do much else
	}
}

metal_buffer::metal_buffer(const device_queue& cqueue,
						   id <MTLBuffer> external_buffer,
						   std::span<uint8_t> host_data_,
						   const MEMORY_FLAG flags_) :
device_buffer(cqueue, [external_buffer length], host_data_, flags_), buffer(external_buffer), is_external(true) {
	// size _has_ to match and be valid/compatible (device_buffer will try to fix the size, but it's obviously an external object)
	// -> detect size mismatch and bail out
	if(size != [external_buffer length]) {
		log_error("can't wrap a buffer that has an incompatible size/length!");
		return;
	}
	if(size < min_multiple()) return;
	
	// device must match
	if(((const metal_device&)cqueue.get_device()).device != [external_buffer device]) {
		log_error("specified Metal device does not match the device set in the external buffer");
		return;
	}
	
	// copy existing options
	options = [buffer cpuCacheMode];
	
	if (has_flag<MEMORY_FLAG::NO_RESOURCE_TRACKING>(flags) ||
		has_flag<DEVICE_CONTEXT_FLAGS::NO_RESOURCE_TRACKING>(dev.context->get_context_flags())) {
		options |= MTLResourceHazardTrackingModeUntracked;
	}
	
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(switch) // MTLStorageModeManaged can't be handled on iOS
#endif
	
	switch([buffer storageMode]) {
		default:
		case MTLStorageModeShared:
			options |= MTLResourceStorageModeShared;
			break;
		case MTLStorageModePrivate:
			options |= MTLResourceStorageModePrivate;
			break;
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		case MTLStorageModeManaged:
			options |= MTLResourceStorageModeManaged;
			break;
#endif
	}
	
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
FLOOR_POP_WARNINGS()
#endif
}

bool metal_buffer::create_internal(const bool copy_host_data, const device_queue& cqueue) {
	const auto& mtl_dev = (const metal_device&)cqueue.get_device();
	
	// should not be called under that condition, but just to be safe
	if (is_external) {
		log_error("buffer is external!");
		return false;
	}
	
	@autoreleasepool {
		// -> use host memory
		if (has_flag<MEMORY_FLAG::USE_HOST_MEMORY>(flags)) {
			buffer = [mtl_dev.device newBufferWithBytesNoCopy:host_data.data() length:host_data.size_bytes() options:options
												  deallocator:nil /* opt-out */];
			assert(!is_heap_buffer);
			return true;
		}
		
		const auto is_private = (options & MTLResourceStorageModeMask) == MTLResourceStorageModePrivate;
		const auto is_shared = (options & MTLResourceStorageModeMask) == MTLResourceStorageModeShared;
		
		// if we want a heap allocated buffer, try to do this first, since we may need to fall back to a normal buffer allocation
		if (is_heap_buffer) {
			assert((is_private && mtl_dev.heap_private) || (is_shared && mtl_dev.heap_shared));
			buffer = [(is_private ? mtl_dev.heap_private : mtl_dev.heap_shared) newBufferWithLength:size options:options];
			if (buffer == nil) {
#if defined(FLOOR_DEBUG)
				log_warn("failed to allocate heap buffer");
#endif
				is_heap_buffer = false;
			} else {
				assert([buffer heap] && [buffer heap] == (is_private ? mtl_dev.heap_private : mtl_dev.heap_shared));
			}
		}
		
		// -> alloc and use device memory
		// copy host memory to device if it is non-null and NO_INITIAL_COPY is not specified
		if (copy_host_data && host_data.data() != nullptr && !has_flag<MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
			// can't use "newBufferWithBytes" with private storage memory
			if (is_private) {
				// -> create the uninitialized private storage buffer and a host memory buffer (or use the staging buffer
				//    if available), then blit from the host memory buffer
				if (!buffer) {
					buffer = [mtl_dev.device newBufferWithLength:size options:options];
				}
				if (staging_buffer == nullptr) {
					MTLResourceOptions host_mem_storage = MTLResourceStorageModeShared;
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
					if (!dev.unified_memory) {
						host_mem_storage = MTLResourceStorageModeManaged;
					}
#endif
					auto buffer_with_host_mem = [mtl_dev.device newBufferWithBytes:host_data.data()
																			length:host_data.size_bytes()
																		   options:(host_mem_storage |
																					MTLResourceCPUCacheModeWriteCombined)];
					if (!buffer_with_host_mem) {
						log_error("failed to allocate memory of size $'", size);
						return false;
					}
					
					id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
					id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
					[blit_encoder copyFromBuffer:floor_force_nonnull(buffer_with_host_mem)
									sourceOffset:0
										toBuffer:buffer
							   destinationOffset:0
											size:size];
					[blit_encoder endEncoding];
					[cmd_buffer commit];
					[cmd_buffer waitUntilCompleted];
					
					[buffer_with_host_mem setPurgeableState:MTLPurgeableStateEmpty];
					buffer_with_host_mem = nil;
				} else {
					staging_buffer->write(cqueue, host_data.data(), size);
					copy(cqueue, *staging_buffer);
				}
			} else {
				// all other storage modes can just use it
				if (!buffer) {
					buffer = [mtl_dev.device newBufferWithBytes:host_data.data() length:host_data.size_bytes() options:options];
				} else {
					// -> heap allocated: need to write data manually
					write(cqueue, host_data.data(), host_data.size_bytes());
				}
			}
		}
		// else: just create a buffer of the specified size (if we don't have already created a heap-allocated one)
		else if (!buffer) {
			buffer = [mtl_dev.device newBufferWithLength:size options:options];
		}
		
		return true;
	}
}

metal_buffer::~metal_buffer() {
#if FLOOR_METAL_INTERNAL_MEM_TRACKING_DEBUGGING
	GUARD(metal_mem_tracking_lock);
	uint64_t pre_alloc_size = 0u;
	uint64_t buffer_alloc_size = 0u;
	if (buffer) {
		pre_alloc_size = [((const metal_device&)dev).device currentAllocatedSize];
		buffer_alloc_size = [buffer allocatedSize];
		if (size > buffer_alloc_size) {
			log_error("buffer size $' > alloc size $'", size, buffer_alloc_size);
		}
	}
#endif
	@autoreleasepool {
		// kill the buffer / auto-release
		if (buffer) {
			[buffer removeAllDebugMarkers];
			if (!is_heap_buffer) {
				[buffer setPurgeableState:MTLPurgeableStateEmpty];
			}
		}
		buffer = nil;
	}
#if FLOOR_METAL_INTERNAL_MEM_TRACKING_DEBUGGING
	if (pre_alloc_size > 0) {
		std::this_thread::yield();
		const auto post_alloc_size = [((const metal_device&)dev).device currentAllocatedSize];
		const auto alloc_diff = (pre_alloc_size >= post_alloc_size ? pre_alloc_size - post_alloc_size : 0);
		if (alloc_diff != buffer_alloc_size && alloc_diff != const_math::round_next_multiple(buffer_alloc_size, 16384ull) &&
			buffer_alloc_size >= 16384u) {
			metal_mem_tracking_leak_total += buffer_alloc_size;
			log_error("buffer ($) dealloc mismatch: expected $' to have been freed, got $' -> leak total $'",
					  debug_label, size, alloc_diff, metal_mem_tracking_leak_total);
		}
	}
#endif
}

void metal_buffer::read(const device_queue& cqueue, const size_t size_, const size_t offset) const {
	read(cqueue, host_data.data(), size_, offset);
}

void metal_buffer::read(const device_queue& cqueue, void* dst, const size_t size_, const size_t offset) const {
	if(buffer == nil) return;
	
	const size_t read_size = (size_ == 0 ? size : size_);
	if(!read_check(size, read_size, offset, flags)) return;
	
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
	if (staging_buffer != nullptr) {
		staging_buffer->copy(cqueue, *this, read_size, offset, offset);
		staging_buffer->read(cqueue, dst, read_size, offset);
		return;
	}
#endif
	
	if (metal_resource_type_needs_sync(options)) {
		sync_metal_resource(cqueue, buffer);
	}
	
	GUARD(lock);
	memcpy(dst, (uint8_t*)[buffer contents] + offset, read_size);
}

void metal_buffer::write(const device_queue& cqueue, const size_t size_, const size_t offset) {
	write(cqueue, host_data.data(), size_, offset);
}

void metal_buffer::write(const device_queue& cqueue floor_unused_on_ios_and_visionos, const void* src,
						 const size_t size_, const size_t offset) {
	if(buffer == nil) return;
	
	const size_t write_size = (size_ == 0 ? size : size_);
	if(!write_check(size, write_size, offset, flags)) return;
	
	@autoreleasepool {
		GUARD(lock);
		id <MTLBuffer> write_buffer = (staging_buffer != nullptr ? staging_buffer->get_metal_buffer() : buffer);
		memcpy((uint8_t*)[write_buffer contents] + offset, src, write_size);
		
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		if ((options & MTLResourceStorageModeMask) == MTLResourceStorageModeManaged ||
			staging_buffer != nullptr) {
			[write_buffer didModifyRange:NSRange { offset, offset + write_size }];
		}
		
		if (staging_buffer != nullptr) {
			copy(cqueue, *staging_buffer, write_size, offset, offset);
		}
#endif
		
		write_buffer = nil;
	}
}

void metal_buffer::copy(const device_queue& cqueue, const device_buffer& src,
						const size_t size_, const size_t src_offset, const size_t dst_offset) {
	if(buffer == nil) return;
	
	const auto& src_mtl_buffer = (const metal_buffer&)src;
	const size_t src_size = src.get_size();
	const size_t copy_size = (size_ == 0 ? std::min(src_size, size) : size_);
	if(!copy_check(size, src_size, copy_size, dst_offset, src_offset)) return;
	
	GUARD(lock);
	@autoreleasepool {
		id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
		id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
		[blit_encoder copyFromBuffer:src_mtl_buffer.get_metal_buffer()
						sourceOffset:src_offset
							toBuffer:buffer
				   destinationOffset:dst_offset
								size:copy_size];
		[blit_encoder endEncoding];
		[cmd_buffer commit];
		[cmd_buffer waitUntilCompleted];
	}
}

bool metal_buffer::fill(const device_queue& cqueue,
						const void* pattern, const size_t& pattern_size,
						const size_t size_, const size_t offset) {
	if(buffer == nil) return false;
	
	const size_t fill_size = (size_ == 0 ? size : size_);
	if(!fill_check(size, fill_size, pattern_size, offset)) return false;
	
	@autoreleasepool {
		GUARD(lock);
		
		const auto fill_single_byte = [&cqueue, &offset, &fill_size, &pattern, this]() {
			@autoreleasepool {
				id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
				id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
				// NOTE: always use "buffer" here, not the staging buffer if available!
				[blit_encoder fillBuffer:buffer
								   range:NSRange { offset, offset + fill_size }
								   value:*(const uint8_t*)pattern];
				[blit_encoder endEncoding];
				[cmd_buffer commit];
				[cmd_buffer waitUntilCompleted];
			}
		};
		
		id <MTLBuffer> fill_buffer = (staging_buffer != nullptr ? staging_buffer->get_metal_buffer() : buffer);
		const size_t pattern_count = fill_size / pattern_size;
		if ((options & MTLResourceStorageModeMask) == MTLResourceStorageModePrivate) {
			switch (pattern_size) {
				case 1:
					// can use fillBuffer directly on the private storage buffer
					fill_single_byte();
					return true;
				default: {
					// there is no fast path -> create a host buffer with the pattern and upload it
					const size_t pattern_fill_size = pattern_size * pattern_count;
					auto pattern_buffer = std::make_unique<uint8_t[]>(pattern_fill_size);
					uint8_t* write_ptr = pattern_buffer.get();
					for (size_t i = 0; i < pattern_count; i++) {
						memcpy(write_ptr, pattern, pattern_size);
						write_ptr += pattern_size;
					}
					write(cqueue, pattern_buffer.get(), pattern_fill_size, offset);
					return true;
				}
			}
		} else {
			switch(pattern_size) {
				case 1:
					fill_single_byte();
					break;
				case 2:
					std::fill_n((uint16_t*)[fill_buffer contents] + offset / 2u, pattern_count, *(const uint16_t*)pattern);
					break;
				case 4:
					std::fill_n((uint32_t*)[fill_buffer contents] + offset / 4u, pattern_count, *(const uint32_t*)pattern);
					break;
				default:
					// not a pattern size that allows a fast memset
					uint8_t* write_ptr = ((uint8_t*)[fill_buffer contents]) + offset;
					for(size_t i = 0; i < pattern_count; i++) {
						memcpy(write_ptr, pattern, pattern_size);
						write_ptr += pattern_size;
					}
					break;
			}
		}
		
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		if ((options & MTLResourceStorageModeMask) == MTLResourceStorageModeManaged ||
			staging_buffer != nullptr) {
			[fill_buffer didModifyRange:NSRange { offset, offset + fill_size }];
		}
		
		if (staging_buffer != nullptr) {
			copy(cqueue, *staging_buffer, fill_size, offset, offset);
		}
#endif
		
		fill_buffer = nil;
	}
	
	return true;
}

bool metal_buffer::zero(const device_queue& cqueue) {
	const uint8_t zero_pattern = 0u;
	return fill(cqueue, &zero_pattern, sizeof(zero_pattern), 0, 0);
}

void* __attribute__((aligned(128))) metal_buffer::map(const device_queue& cqueue,
													  const MEMORY_MAP_FLAG flags_,
													  const size_t size_, const size_t offset) NO_THREAD_SAFETY_ANALYSIS {
	if(buffer == nil) return nullptr;
	
	const size_t map_size = (size_ == 0 ? size : size_);
	if(!map_check(size, map_size, flags, flags_, offset)) return nullptr;
	
	bool does_read = false, does_write = false;
	if(has_flag<MEMORY_MAP_FLAG::WRITE_INVALIDATE>(flags_)) {
		does_write = true;
	}
	else {
		switch(flags_ & MEMORY_MAP_FLAG::READ_WRITE) {
			case MEMORY_MAP_FLAG::READ:
				does_read = true;
				break;
			case MEMORY_MAP_FLAG::WRITE:
				does_write = true;
				break;
			case MEMORY_MAP_FLAG::READ_WRITE:
				does_read = true;
				does_write = true;
				break;
			case MEMORY_MAP_FLAG::NONE:
			default:
				log_error("neither read nor write flag set for buffer mapping!");
				return nullptr;
		}
	}
	
	// must lock this to make sure all prior work has completed
	_lock();
	
	// NOTE: MTLResourceStorageModePrivate handled by map_check (-> no host access is handled)
	const bool write_only = (!does_read && does_write);
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
	const bool read_only = (does_read && !does_write);
	if((options & MTLResourceStorageModeMask) == MTLResourceStorageModeManaged) {
		aligned_ptr<uint8_t> alloc_host_buffer;
		alignas(128) unsigned char* host_buffer = nullptr;
		
		// check if we need to copy the buffer from the device (in case READ was specified)
		if (!write_only) {
			// need to sync buffer (resource) before being able to read it
			sync_metal_resource(cqueue, buffer);
			
			// direct access to the Metal buffer
			host_buffer = ((uint8_t*)[buffer contents]) + offset;
		} else {
			// if write-only, we don't need to actually access the buffer just yet (no need to sync),
			// so just alloc host memory that can be accessed/used by the user
			alloc_host_buffer = make_aligned_ptr<uint8_t>(map_size);
			host_buffer = alloc_host_buffer.get();
		}
		
		// need to remember how much we mapped and where (so the host->device write-back copies the right amount of bytes)
		mappings.emplace(host_buffer, metal_mapping { std::move(alloc_host_buffer), map_size, offset, flags_, write_only, read_only });
		
		return host_buffer;
	}
	else if(staging_buffer != nullptr) {
		if(!write_only) {
			// read-only or read-write: update staging buffer
			staging_buffer->copy(cqueue, (metal_buffer&)*this, map_size, offset);
		}
		
		// do the mapping using the staging buffer
		auto mapped_ptr = staging_buffer->map(cqueue, flags_, size_, offset);
		
		// remember the mapping so that we can properly unmap again later
		mappings.emplace(mapped_ptr, metal_mapping { {}, map_size, offset, flags_, write_only, read_only });
		
		return mapped_ptr;
	}
	else {
#endif
		if (!write_only && metal_resource_type_needs_sync(options)) {
			// need to sync buffer (resource) before being able to read it
			sync_metal_resource(cqueue, buffer);
		}
		// can just return the CPU mapped pointer
		return (void*)(((uint8_t*)[buffer contents]) + offset);
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
	}
#endif
}

bool metal_buffer::unmap(const device_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) NO_THREAD_SAFETY_ANALYSIS {
	if(buffer == nil) return false;
	if(mapped_ptr == nullptr) return false;
	
	bool success = true;
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
	const bool is_managed = ((options & MTLResourceStorageModeMask) == MTLResourceStorageModeManaged);
	const bool has_staging_buffer = (staging_buffer != nullptr);
	if (is_managed || has_staging_buffer) {
		// check if this is actually a mapped pointer (+get the mapped size)
		const auto iter = mappings.find(mapped_ptr);
		if (iter == mappings.end()) {
			log_error("invalid mapped pointer: $X", mapped_ptr);
			return false;
		}
		
		if (is_managed) {
			// check if we need to actually copy data back to the device (not the case if read-only mapping)
			if (has_flag<MEMORY_MAP_FLAG::WRITE>(iter->second.flags) ||
				has_flag<MEMORY_MAP_FLAG::WRITE_INVALIDATE>(iter->second.flags)) {
				// if mapping is write-only, mapped_ptr was manually alloc'ed and we need to copy the data
				// to the actual Metal buffer
				if(iter->second.write_only) {
					memcpy(((uint8_t*)[buffer contents]) + iter->second.offset, mapped_ptr, iter->second.size);
				}
				// else: the user received a pointer directly to the Metal buffer and nothing needs to be copied
				
				// finally, notify the buffer that we changed its contents
				[buffer didModifyRange:NSRange { iter->second.offset, iter->second.offset + iter->second.size }];
			}
		} else if(has_staging_buffer) {
			// perform unmap on the staging buffer, then update this buffer if necessary
			success = staging_buffer->unmap(cqueue, mapped_ptr);
			if (iter->second.write_only || !iter->second.read_only) {
				copy(cqueue, *staging_buffer, iter->second.size, iter->second.offset);
			}
		}
		
		// remove the mapping
		mappings.erase(mapped_ptr);
	}
#endif
	
	if ((options & MTLResourceStorageModeMask) == MTLResourceStorageModeShared) {
		sync_metal_resource(cqueue, buffer);
	}
	
	_unlock();
	
	return success;
}

void metal_buffer::sync_metal_resource(const device_queue& cqueue, id <MTLResource> __unsafe_unretained floor_nonnull rsrc) {
	@autoreleasepool {
#if defined(FLOOR_DEBUG)
		if (!metal_resource_type_needs_sync([rsrc storageMode])) {
			log_error("only call this with managed/shared memory");
			return;
		}
#endif
		if (([rsrc storageMode] & MTLResourceStorageModeMask) == MTLResourceStorageModeShared) {
			// for shared storage: just wait until all previously queued command buffers have executed
			cqueue.finish();
			return;
		}
		
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
		id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
		[blit_encoder synchronizeResource:rsrc];
		[blit_encoder endEncoding];
		[cmd_buffer commit];
		[cmd_buffer waitUntilCompleted];
#endif
	}
}

void metal_buffer::set_debug_label(const std::string& label) {
	device_memory::set_debug_label(label);
	if (buffer) {
		@autoreleasepool {
			buffer.label = [NSString stringWithUTF8String:debug_label.c_str()];
		}
	}
}

const device_buffer* metal_buffer::get_null_buffer(const device& dev_) {
	return ((const metal_context*)dev_.context)->get_null_buffer(dev_);
}

} // namespace fl

#endif
