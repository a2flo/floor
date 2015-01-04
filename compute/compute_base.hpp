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

#ifndef __FLOOR_COMPUTE_BASE_HPP__
#define __FLOOR_COMPUTE_BASE_HPP__

#include <unordered_set>

#include <floor/core/logger.hpp>
#include <floor/compute/compute_common.hpp>
#include <floor/compute/compute_buffer.hpp>
#include <floor/compute/compute_device.hpp>
#include <floor/compute/compute_kernel.hpp>
#include <floor/compute/compute_queue.hpp>
#include <floor/compute/compute_program.hpp>

// necessary here, because there are no out-of-line virtual method definitions
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

//! pure abstract base class that provides the interface for all compute implementations (opencl, cuda, ...)
class compute_base {
public:
	//////////////////////////////////////////
	// init / context creation
	
	//! unfortunately necessary, has empty body in .cpp
	virtual ~compute_base() = 0;
	
	//! initializes the compute context/object
	virtual void init(const bool use_platform_devices = false,
					  const uint32_t platform_index = ~0u,
					  const bool gl_sharing = false,
					  const unordered_set<string> device_restriction = {}) = 0;
	
	//! returns true if there is compute support (i.e. a compute context could be created and available compute devices exist)
	virtual bool is_supported() const = 0;
	
	//! returns the underlying compute implementation type
	virtual COMPUTE_TYPE get_compute_type() const = 0;
	
	//////////////////////////////////////////
	// enums
	
	//! opencl and cuda platform vendors
	enum class PLATFORM_VENDOR : uint32_t {
		NVIDIA,
		INTEL,
		AMD,
		APPLE,
		FREEOCL,
		POCL,
		CUDA,
		UNKNOWN
	};
	
	//! returns a string representation of the specified PLATFORM_VENDOR enum
	static constexpr const char* platform_vendor_to_str(const PLATFORM_VENDOR& pvendor) {
		switch(pvendor) {
			case PLATFORM_VENDOR::NVIDIA: return "NVIDIA";
			case PLATFORM_VENDOR::INTEL: return "INTEL";
			case PLATFORM_VENDOR::AMD: return "AMD";
			case PLATFORM_VENDOR::APPLE: return "APPLE";
			case PLATFORM_VENDOR::FREEOCL: return "FREEOCL";
			case PLATFORM_VENDOR::POCL: return "POCL";
			case PLATFORM_VENDOR::CUDA: return "CUDA";
			case PLATFORM_VENDOR::UNKNOWN: break;
		}
		return "UNKNOWN";
	}
	
	//////////////////////////////////////////
	// device functions
	
	//! returns the array of all valid devices in this context
	const vector<shared_ptr<compute_device>>& get_devices() const {
		return devices;
	}
	
	//! tries to return the device matching the specified "type"
	//! NOTE: will return any valid device if none matches "type"!
	shared_ptr<compute_device> get_device(const compute_device::TYPE type) const;
	
	//! creates and returns a compute_queue (aka command queue or stream) for the specified device
	virtual shared_ptr<compute_queue> create_queue(shared_ptr<compute_device> dev) = 0;
	
	//////////////////////////////////////////
	// buffer creation
	
	//! constructs an uninitialized buffer of the specified size
	virtual shared_ptr<compute_buffer> create_buffer(const size_t& size,
													 const COMPUTE_BUFFER_FLAG flags = (COMPUTE_BUFFER_FLAG::READ_WRITE |
																						COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) = 0;
	
	//! constructs a buffer of the specified size, using the host pointer as specified by the flags
	virtual shared_ptr<compute_buffer> create_buffer(const size_t& size,
													 void* data,
													 const COMPUTE_BUFFER_FLAG flags = (COMPUTE_BUFFER_FLAG::READ_WRITE |
																						COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) = 0;
	
	//! constructs a buffer of the specified data (under consideration of the specified flags)
	template <typename data_type>
	shared_ptr<compute_buffer> create_buffer(const vector<data_type>& data,
											 const COMPUTE_BUFFER_FLAG flags = (COMPUTE_BUFFER_FLAG::READ_WRITE |
																				COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) {
		return create_buffer(sizeof(data_type) * data.size(), (void*)&data[0], flags);
	}
	
	//! constructs a buffer of the specified data (under consideration of the specified flags)
	template <typename data_type, size_t n>
	shared_ptr<compute_buffer> create_buffer(const array<data_type, n>& data,
											 const COMPUTE_BUFFER_FLAG flags = (COMPUTE_BUFFER_FLAG::READ_WRITE |
																				COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) {
		return create_buffer(sizeof(data_type) * n, (void*)&data[0], flags);
	}
	
	//////////////////////////////////////////
	// basic control functions
	
	//! block until all currently scheduled kernels have been executed
	virtual void finish() = 0;
	
	//! flush all prior
	virtual void flush() = 0;
	
	//! TODO: figure out how to do this in cuda
	//virtual void barrier() = 0;
	
	//! makes the compute context active in the current thread
	virtual void activate_context() = 0;
	
	//! makes the compute context inactive in the current thread
	virtual void deactivate_context() = 0;
	
	//////////////////////////////////////////
	// program/kernel functionality
	
	//! adds and compiles a program and its kernels from a file
	virtual weak_ptr<compute_program> add_program_file(const string& file_name,
													   const string additional_options = "") = 0;
	
	//! adds and compiles a program and its kernels from the provided source code
	virtual weak_ptr<compute_program> add_program_source(const string& source_code,
														 const string additional_options = "") = 0;
	
protected:
	//! platform vendor enum (set after initialization)
	PLATFORM_VENDOR platform_vendor { PLATFORM_VENDOR::UNKNOWN };
	
	//! true if compute support (set after initialization)
	bool supported { false };
	
	//! all compute devices of the current compute context
	vector<shared_ptr<compute_device>> devices;
	//! pointer to the fastest (any) compute_device if it exists
	shared_ptr<compute_device> fastest_device;
	//! pointer to the fastest cpu compute_device if it exists
	shared_ptr<compute_device> fastest_cpu_device;
	//! pointer to the fastest gpu compute_device if it exists
	shared_ptr<compute_device> fastest_gpu_device;
	
	//! all compute queues of the current compute context
	vector<shared_ptr<compute_queue>> queues;
	
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
