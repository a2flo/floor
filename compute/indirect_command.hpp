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

#pragma once

#include <floor/core/essentials.hpp>
#include <floor/core/cpp_ext.hpp>
#include <floor/constexpr/ext_traits.hpp>
#include <floor/compute/compute_device.hpp>
#include <floor/compute/compute_kernel.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/graphics/graphics_index_type.hpp>

class compute_buffer;
class graphics_pipeline;

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

//! full description used to create a indirect command pipeline
struct indirect_command_description {
	//! allowed command type
	enum class COMMAND_TYPE {
		COMPUTE,
		RENDER,
	};
	//! specifies the type of commands that may be encoded
	//! NOTE: compute and render commands can not be encoded in the same indirect command pipeline
	COMMAND_TYPE command_type { COMMAND_TYPE::RENDER };
	
	//! the max amount of commands that may be encoded in the indirect command pipeline
	//! NOTE: must at least be one
	//! NOTE: different backends may have a different max limit (Metal: 16384)
	uint32_t max_command_count { 1u };
	
	//! the max amount of buffers that can be set/used in a kernel function that is encoded by a compute command
	uint32_t max_kernel_buffer_count { 0u };
	
	//! the max amount of buffers that can be set/used in a vertex function that is encoded by a render command
	uint32_t max_vertex_buffer_count { 0u };
	
	//! the max amount of buffers that can be set/used in a fragment function that is encoded by a render command
	uint32_t max_fragment_buffer_count { 0u };
	
	//! automatically computes the max kernel/vertex/fragment buffer counts for the specified device and listed "functions"
	void compute_buffer_counts_from_functions(const compute_device& dev, const vector<const compute_kernel*>& functions);
	
	//! sets the debug label for indirect commands created from this description (e.g. for display in a debugger)
	string debug_label;
	
	//! if set, this ignores the backend specific fixed max command count limit (use at your own risk)
	//! NOTE: Vulkan technically has no limit anyways and on Metal it is currently unclear what the actual limit is
	bool ignore_max_max_command_count_limit { false };
};

class indirect_command_encoder;
class indirect_render_command_encoder;
class indirect_compute_command_encoder;

//! stores and manages one or more indirect compute/render command(s)
//! TODO: allow direct indexed access for individual command resets?
class indirect_command_pipeline {
public:
	explicit indirect_command_pipeline(const indirect_command_description& desc_);
	virtual ~indirect_command_pipeline() = default;
	
	//! returns the description of this pipeline
	const indirect_command_description& get_description() const {
		return desc;
	}
	
	//! returns true if this pipeline is in a valid state
	bool is_valid() const {
		return valid;
	}
	
	//! returns the amount of commands that have actually been encoded in this pipeline
	uint32_t get_command_count() const;
	
	//! adds a new render command this indirect command pipeline,
	//! returning a reference to the non-owning encoder object that can be used to encode the render command
	virtual indirect_render_command_encoder& add_render_command(const compute_device& dev,
																const graphics_pipeline& pipeline,
																const bool is_multi_view = false) = 0;
	
	//! adds a new compute command this indirect command pipeline,
	//! returning a reference to the non-owning encoder object that can be used to encode the compute command
	virtual indirect_compute_command_encoder& add_compute_command(const compute_device& dev,
																  const compute_kernel& kernel_obj) = 0;
	
	//! completes this indirect command pipeline for the specified device
	virtual void complete(const compute_device& dev) = 0;
	//! completes this indirect command pipeline for all devices
	virtual void complete() = 0;
	
	//! resets/removes all encoded indirect commands in this pipeline
	//! NOTE: must call complete() again after encoding new indirect commands
	virtual void reset();
	
protected:
	const indirect_command_description desc;
	bool valid { true };
	
	vector<unique_ptr<indirect_command_encoder>> commands;
	
};

//! generic base class/functions for encoding render/compute commands
class indirect_command_encoder {
protected:
	//! checks if an individual argument type is valid
	//! NOTE: may be any of:
	//!  * compute_buffer*, argument_buffer*, or derived
	//!  * unique_ptr<compute_buffer>, unique_ptr<argument_buffer>, or derived
	//!  * shared_ptr<compute_buffer>, shared_ptr<argument_buffer>, or derived
	template <typename T>
	static constexpr bool check_arg_type() {
		using decayed_type = decay_t<T>;
		if constexpr (is_pointer_v<decayed_type> &&
					  (is_base_of_v<compute_buffer, remove_pointer_t<decayed_type>> ||
					   is_base_of_v<argument_buffer, remove_pointer_t<decayed_type>>)) {
			return true;
		} else if constexpr (ext::is_shared_ptr_v<decayed_type> || ext::is_unique_ptr_v<decayed_type>) {
			using pointee_type = typename decayed_type::element_type;
			if constexpr (is_base_of_v<compute_buffer, pointee_type> ||
						  is_base_of_v<argument_buffer, pointee_type>) {
				return true;
			} else {
				instantiation_trap("invalid shared_ptr/unique_ptr argument type, pointee type must be a compute_buffer or argument_buffer");
				return false;
			}
		} else {
			instantiation_trap("invalid argument type, only compute_buffer and argument_buffer types are allowed in indirect commands");
			return false;
		}
	}
	
	//! checks if all argument types are valid
	template <typename... Args>
	static constexpr bool check_arg_types() {
		if constexpr (sizeof...(Args) == 0) {
			instantiation_trap("must specify at least one argument");
		} else {
			return (... && check_arg_type<Args>());
		}
		return false;
	}
	
public:
	explicit indirect_command_encoder(const compute_device& dev_) : dev(dev_) {}
	virtual ~indirect_command_encoder() = default;
	
	//! returns the associated device for this indirect_command_encoder
	const compute_device& get_device() const {
		return dev;
	}
	
protected:
	//! device needed for encoding
	const compute_device& dev;
	
	//! sets/encodes the specified arguments in this command
	virtual void set_arguments_vector(vector<compute_kernel_arg>&& args) = 0;
	
};

//! encoder for encoding render commands in an indirect command pipeline
class indirect_render_command_encoder : public indirect_command_encoder {
public:
	//! NOTE: compute_device and graphics_pipeline must be valid for the lifetime of the parent indirect_command_pipeline
	indirect_render_command_encoder(const compute_device& dev_, const graphics_pipeline& pipeline_, const bool is_multi_view_);
	~indirect_render_command_encoder() override = default;
	
	//! encode a simple draw call using the specified parameters
	//! NOTE: returns *this to enable subsequent set_arguments()
	virtual indirect_render_command_encoder& draw(const uint32_t vertex_count,
												  const uint32_t instance_count = 1u,
												  const uint32_t first_vertex = 0u,
												  const uint32_t first_instance = 0u) = 0;
	
	//! encode an indexed draw call using the specified parameters
	//! NOTE: returns *this to enable subsequent set_arguments()
	virtual indirect_render_command_encoder& draw_indexed(const compute_buffer& index_buffer,
														  const uint32_t index_count,
														  const uint32_t instance_count = 1u,
														  const uint32_t first_index = 0u,
														  const int32_t vertex_offset = 0u,
														  const uint32_t first_instance = 0u,
														  const INDEX_TYPE index_type = INDEX_TYPE::UINT) = 0;
	
	//! encodes a patch draw call using the specified parameters
	//! NOTE: returns *this to enable subsequent set_arguments()
	virtual indirect_render_command_encoder& draw_patches(const vector<const compute_buffer*> control_point_buffers,
														  const compute_buffer& tessellation_factors_buffer,
														  const uint32_t patch_control_point_count,
														  const uint32_t patch_count,
														  const uint32_t first_patch = 0u,
														  const uint32_t instance_count = 1u,
														  const uint32_t first_instance = 0u) = 0;
	
	//! encodes an indexed patch draw call using the specified parameters
	//! NOTE: returns *this to enable subsequent set_arguments()
	virtual indirect_render_command_encoder& draw_patches_indexed(const vector<const compute_buffer*> control_point_buffers,
																  const compute_buffer& control_point_index_buffer,
																  const compute_buffer& tessellation_factors_buffer,
																  const uint32_t patch_control_point_count,
																  const uint32_t patch_count,
																  const uint32_t first_index = 0u,
																  const uint32_t first_patch = 0u,
																  const uint32_t instance_count = 1u,
																  const uint32_t first_instance = 0u) = 0;
	
	//! sets/encodes the specified arguments in this command
	//! NOTE: vertex shader arguments are specified first, fragment shader arguments after
	//! NOTE: it is only allowed to encode/use buffer-type parameters, i.e. no images or const-parameters are allowed
	//!       -> only use buffer<T> and arg_buffer<T> parameters in vertex/fragment/kernel functions
	template <typename... Args>
	indirect_render_command_encoder& set_arguments(const Args&... args)
	__attribute__((enable_if(indirect_command_encoder::check_arg_types<Args...>(), "valid args"))) {
		set_arguments_vector({ args... });
		return *this;
	}
	
	//! sets/encodes the specified arguments in this command
	template <typename... Args>
	indirect_render_command_encoder& set_arguments(const Args&...)
	__attribute__((enable_if(!indirect_command_encoder::check_arg_types<Args...>(), "invalid args"), unavailable("invalid argument(s)!")));
	
protected:
	const graphics_pipeline& pipeline;
	const bool is_multi_view { false };
};

//! encoder for encoding compute commands in an indirect command pipeline
class indirect_compute_command_encoder : public indirect_command_encoder {
public:
	//! NOTE: compute_device and compute_kernel must be valid for the lifetime of the parent indirect_command_pipeline
	indirect_compute_command_encoder(const compute_device& dev_, const compute_kernel& kernel_obj_);
	~indirect_compute_command_encoder() override = default;
	
	//! encode a 1D kernel execution using the specified parameters
	//! NOTE: returns *this to enable subsequent set_arguments()
	indirect_compute_command_encoder& execute(const uint32_t& global_work_size,
											  const uint32_t& local_work_size) {
		return execute(1u, uint3 { global_work_size, 1u, 1u }, uint3 { local_work_size, 1u, 1u });
	}
	
	//! encode a 2D kernel execution using the specified parameters
	//! NOTE: returns *this to enable subsequent set_arguments()
	indirect_compute_command_encoder& execute(const uint2& global_work_size,
											  const uint2& local_work_size) {
		return execute(2u, uint3 { global_work_size.x, global_work_size.y, 1u }, uint3 { local_work_size.x, local_work_size.y, 1u });
	}
	
	//! encode a 3D kernel execution using the specified parameters
	//! NOTE: returns *this to enable subsequent set_arguments()
	indirect_compute_command_encoder& execute(const uint3& global_work_size,
											  const uint3& local_work_size) {
		return execute(3u, global_work_size, local_work_size);
	}
	
	//! encodes a barrier at the current location:
	//! this ensures that all kernel executions before this barrier have finished execution, before any kernel executions past this point may begin
	virtual indirect_compute_command_encoder& barrier() = 0;
	
	//! sets/encodes the specified arguments in this command
	//! NOTE: it is only allowed to encode/use buffer-type parameters, i.e. no images or const-parameters are allowed
	//!       -> only use buffer<T> and arg_buffer<T> parameters in vertex/fragment/kernel functions
	template <typename... Args>
	indirect_compute_command_encoder& set_arguments(const Args&... args)
	__attribute__((enable_if(indirect_command_encoder::check_arg_types<Args...>(), "valid args"))) {
		set_arguments_vector({ args... });
		return *this;
	}
	
	//! sets/encodes the specified arguments in this command
	template <typename... Args>
	indirect_compute_command_encoder& set_arguments(const Args&...)
	__attribute__((enable_if(!indirect_command_encoder::check_arg_types<Args...>(), "invalid args"), unavailable("invalid argument(s)!")));
	
protected:
	const compute_kernel& kernel_obj;
	const compute_kernel::kernel_entry* entry { nullptr };
	
	virtual indirect_compute_command_encoder& execute(const uint32_t dim,
													  const uint3& global_work_size,
													  const uint3& local_work_size) = 0;
	
};

FLOOR_POP_WARNINGS()
