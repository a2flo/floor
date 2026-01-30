/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#include <string>
#include <vector>
#include <floor/math/vector_lib.hpp>
#include <floor/device/device_function_arg.hpp>
#include <floor/device/device_common.hpp>
#include <floor/device/device_fence.hpp>

namespace fl {

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class device;
class device_context;
class device_memory;
class device_function;
class indirect_command_pipeline;

class device_queue {
public:
	//! argument type validity specializations, with pretty error messages
	template <typename T> struct is_valid_arg { static constexpr bool valid() { return true; } };
	
	template <typename T> struct is_device_memory_pointer : public std::false_type {};
	template <typename T>
	requires (std::is_pointer_v<T> &&
			  (std::is_base_of_v<device_memory, std::remove_pointer_t<T>> ||
			   std::is_base_of_v<argument_buffer, std::remove_pointer_t<T>>))
	struct is_device_memory_pointer<T> : public std::true_type {};
	
	template <typename T>
	requires (std::is_pointer_v<T> && !is_device_memory_pointer<T>::value)
	struct is_valid_arg<T> {
		static constexpr bool valid()
		__attribute__((unavailable("raw pointers are not allowed"))) { return false; }
	};
	template <typename T>
	requires std::is_null_pointer_v<T>
	struct is_valid_arg<T> {
		static constexpr bool valid()
		__attribute__((unavailable("nullptr is not allowed"))) { return false; }
	};
	
	//! checks if an individual argument type is valid
	template <typename T = void>
	static constexpr bool check_arg_types() {
		constexpr const bool is_valid { is_valid_arg<T>::valid() };
		static_assert(is_valid, "invalid argument type");
		return is_valid;
	}
	
	//! checks if all argument types are valid
	template <typename T, typename... Args> requires (sizeof...(Args) != 0)
	static constexpr bool check_arg_types() {
		return (check_arg_types<T>() && check_arg_types<Args...>());
	}
	
	//! checks if "work_size_type" is a valid work size type (uint1/uint2/uint3)
	template <class work_size_type>
	static constexpr bool is_valid_work_size_type() {
		return (std::is_same_v<work_size_type, uint1> ||
				std::is_same_v<work_size_type, uint2> ||
				std::is_same_v<work_size_type, uint3>);
	}
	
public:
	enum class QUEUE_TYPE {
		//! default queue type
		//! CUDA/OpenCL/Host: compute-only
		//! Metal/Vulkan: graphics + compute support
		ALL,
		//! CUDA/OpenCL/Host/Metal: same as ALL
		//! Vulkan: compute-only
		COMPUTE,
	};
	
	explicit device_queue(const device& dev_, const QUEUE_TYPE queue_type_) : dev(dev_), queue_type(queue_type_) {}
	virtual ~device_queue() = default;
	
	//! blocks until all currently scheduled work in this queue has been executed
	virtual void finish() const = 0;
	
	//! flushes all scheduled work to the associated device
	virtual void flush() const = 0;
	
	//! implementation specific queue object ptr (cl_command_queue or CUStream, both "struct _ *")
	virtual const void* get_queue_ptr() const = 0;
	virtual void* get_queue_ptr() = 0;
	
	//! enqueues (and executes) the specified kernel into this queue
	template <typename... Args, class work_size_type>
	requires (is_valid_work_size_type<work_size_type>())
	void execute(const device_function& kernel,
				 const work_size_type& global_work_size,
				 const work_size_type& local_work_size,
				 const Args&... args) const __attribute__((enable_if(check_arg_types<Args...>(), "valid args"))) {
		kernel_execute_forwarder(kernel, false, false, global_work_size, local_work_size, {}, { args... });
	}
	
	template <typename... Args, class work_size_type>
	requires (is_valid_work_size_type<work_size_type>())
	void execute(const device_function&, const work_size_type&, const work_size_type&, const Args&...) const
	__attribute__((enable_if(!check_arg_types<Args...>(), "invalid args"), unavailable("invalid kernel argument(s)!")));
	
	//! enqueues (and executes) the specified kernel into this queue, calling the specified "completion_handler" on kernel completion
	template <typename... Args, class work_size_type>
	requires (is_valid_work_size_type<work_size_type>())
	void execute(const device_function& kernel,
				 kernel_completion_handler_f&& completion_handler,
				 const work_size_type& global_work_size,
				 const work_size_type& local_work_size,
				 const Args&... args) const __attribute__((enable_if(check_arg_types<Args...>(), "valid args"))) {
		kernel_execute_forwarder(kernel, false, false, global_work_size, local_work_size,
								 std::forward<kernel_completion_handler_f>(completion_handler), { args... });
	}
	
	template <typename... Args, class work_size_type>
	requires (is_valid_work_size_type<work_size_type>())
	void execute(const device_function&, kernel_completion_handler_f&&, const work_size_type&, const work_size_type&, const Args&...) const
	__attribute__((enable_if(!check_arg_types<Args...>(), "invalid args"), unavailable("invalid kernel argument(s)!")));
	
	//! enqueues and executes the specified kernel into this queue, blocking until execution has finished
	template <typename... Args, class work_size_type>
	requires (is_valid_work_size_type<work_size_type>())
	void execute_sync(const device_function& kernel,
					  const work_size_type& global_work_size,
					  const work_size_type& local_work_size,
					  const Args&... args) const __attribute__((enable_if(check_arg_types<Args...>(), "valid args"))) {
		kernel_execute_forwarder(kernel, false, true, global_work_size, local_work_size, {}, { args... });
	}
	
	template <typename... Args, class work_size_type>
	requires (is_valid_work_size_type<work_size_type>())
	void execute_sync(const device_function&, const work_size_type&, const work_size_type&, const Args&...) const
	__attribute__((enable_if(!check_arg_types<Args...>(), "invalid args"), unavailable("invalid kernel argument(s)!")));
	
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
	//! enqueues (and executes cooperatively) the specified kernel into this queue
	//! NOTE: the device/backend this is executed on requires "cooperative_kernel_support"
	template <typename... Args, class work_size_type>
	requires (is_valid_work_size_type<work_size_type>())
	void execute_cooperative(const device_function& kernel,
							 const work_size_type& global_work_size,
							 const work_size_type& local_work_size,
							 const Args&... args) const __attribute__((enable_if(check_arg_types<Args...>(), "valid args"))) {
		kernel_execute_forwarder(kernel, true, false, global_work_size, local_work_size, {}, { args... });
	}
	
	template <typename... Args, class work_size_type>
	requires (is_valid_work_size_type<work_size_type>())
	void execute_cooperative(const device_function&, const work_size_type&, const work_size_type&, const Args&...) const
	__attribute__((enable_if(!check_arg_types<Args...>(), "invalid args"), unavailable("invalid kernel argument(s)!")));
	
	//! enqueues (and executes cooperatively) the specified kernel into this queue, calling the specified "completion_handler" on kernel completion
	//! NOTE: the device/backend this is executed on requires "cooperative_kernel_support"
	template <typename... Args, class work_size_type>
	requires (is_valid_work_size_type<work_size_type>())
	void execute_cooperative(const device_function& kernel,
							 kernel_completion_handler_f&& completion_handler,
							 const work_size_type& global_work_size,
							 const work_size_type& local_work_size,
							 const Args&... args) const __attribute__((enable_if(check_arg_types<Args...>(), "valid args"))) {
		kernel_execute_forwarder(kernel, true, false, global_work_size, local_work_size,
								 std::forward<kernel_completion_handler_f>(completion_handler), { args... });
	}
	
	template <typename... Args, class work_size_type>
	requires (is_valid_work_size_type<work_size_type>())
	void execute_cooperative(const device_function&, kernel_completion_handler_f&&,
							 const work_size_type&, const work_size_type&, const Args&...) const
	__attribute__((enable_if(!check_arg_types<Args...>(), "invalid args"), unavailable("invalid kernel argument(s)!")));
	
	//! enqueues (and executes cooperatively) the specified kernel into this queue, blocking until execution has finished
	//! NOTE: the device/backend this is executed on requires "cooperative_kernel_support"
	template <typename... Args, class work_size_type>
	requires (is_valid_work_size_type<work_size_type>())
	void execute_cooperative_sync(const device_function& kernel,
								  const work_size_type& global_work_size,
								  const work_size_type& local_work_size,
								  const Args&... args) const __attribute__((enable_if(check_arg_types<Args...>(), "valid args"))) {
		kernel_execute_forwarder(kernel, true, true, global_work_size, local_work_size, {}, { args... });
	}
	
	template <typename... Args, class work_size_type>
	requires (is_valid_work_size_type<work_size_type>())
	void execute_cooperative_sync(const device_function&, const work_size_type&, const work_size_type&, const Args&...) const
	__attribute__((enable_if(!check_arg_types<Args...>(), "invalid args"), unavailable("invalid kernel argument(s)!")));
#endif
	
	//! reusable kernel execution parameters
	struct execution_parameters_t {
		//! the execution dimensionality of the kernel: 1/1D, 2/2D or 3/3D
		uint32_t execution_dim { 1u };
		//! global work size (must be non-zero for all dimensions that are executed)
		uint3 global_work_size;
		//! local work size (must be non-zero for all dimensions that are executed)
		uint3 local_work_size;
		//! kernel arguments
		std::vector<device_function_arg> args;
		//! all fences the kernel execution will wait on before execution
		std::vector<const device_fence*> wait_fences;
		//! all fences the kernel will signal once execution has completed
		std::vector<device_fence*> signal_fences;
		//! flag whether this is a cooperative kernel launch
		bool is_cooperative { false };
		//! after enqueueing the kernel, wait until the kernel has finished execution -> execute_with_parameters() becomes blocking
		//! NOTE: since multiple kernel executions might be in-flight in this queue, this is generally more efficient than calling finish()
		bool wait_until_completion { false };
		//! sets the debug label for the kernel execution (e.g. for display in a debugger)
		const char* debug_label { nullptr };
	};
	
	//! enqueues the specified kernel into this queue, using the specified execution parameters
	virtual void execute_with_parameters(const device_function& kernel, const execution_parameters_t& params,
										 kernel_completion_handler_f&& completion_handler = {}) const;
	
	//! reusable indirect compute pipeline execution parameters
	struct indirect_execution_parameters_t {
		//! all fences the indirect compute pipeline execution will wait on before execution
		std::vector<const device_fence*> wait_fences;
		//! all fences the indirect compute pipeline will signal once execution has completed
		std::vector<device_fence*> signal_fences;
		//! after enqueueing the indirect compute pipeline, wait until it has finished execution -> execute_indirect() becomes blocking
		//! NOTE: since multiple kernel/pipeline executions might be in-flight in this queue, this is generally more efficient than calling finish()
		bool wait_until_completion { false };
		//! sets the debug label for the indirect compute pipeline execution (e.g. for display in a debugger)
		const char* debug_label { nullptr };
	};
	
	//! executes the compute commands from an indirect command pipeline, additionally using the specified execution parameters,
	//! calling the specified "completion_handler" on indirect command completion,
	//! executes #"command_count" commands (or all if ~0u) starting at "command_offset" -> all commands by default
	virtual void execute_indirect(const indirect_command_pipeline& indirect_cmd,
								  const indirect_execution_parameters_t& params,
								  kernel_completion_handler_f&& completion_handler = {},
								  const uint32_t command_offset = 0u,
								  const uint32_t command_count = ~0u) const;
	
	//! executes the compute commands from an indirect command pipeline
	//! executes #"command_count" commands (or all if ~0u) starting at "command_offset" -> all commands by default
	void execute_indirect(const indirect_command_pipeline& indirect_cmd,
						  const uint32_t command_offset = 0u,
						  const uint32_t command_count = ~0u) const {
		return execute_indirect(indirect_cmd, {}, {}, command_offset, command_count);
	}
	
	//! returns the device associated with this queue
	const device& get_device() const { return dev; }
	//! returns the context associated with this queue
	const device_context& get_context() const;
	device_context& get_mutable_context() const;
	
	//! returns true if this queue has profiling support
	virtual bool has_profiling_support() const {
		return false;
	}
	
	//! starts profiling
	virtual void start_profiling() const;
	
	//! stops the previously started profiling and returns the elapsed time in microseconds
	virtual uint64_t stop_profiling() const;
	
	//! returns the type of this queue
	QUEUE_TYPE get_queue_type() const {
		return queue_type;
	}
	
	//! sets the debug label of this device queue
	virtual void set_debug_label(const std::string& label [[maybe_unused]]) {}
	
protected:
	const device& dev;
	mutable uint64_t us_prof_start { 0 };
	const QUEUE_TYPE queue_type { QUEUE_TYPE::ALL };
	
	//! internal forwarders to the actual kernel execution implementations
	void kernel_execute_forwarder(const device_function& kernel,
								  const bool is_cooperative,
								  const bool wait_until_completion,
								  const uint1& global_size, const uint1& local_size,
								  kernel_completion_handler_f&& completion_handler,
								  const std::vector<device_function_arg>& args) const;
	void kernel_execute_forwarder(const device_function& kernel,
								  const bool is_cooperative,
								  const bool wait_until_completion,
								  const uint2& global_size, const uint2& local_size,
								  kernel_completion_handler_f&& completion_handler,
								  const std::vector<device_function_arg>& args) const;
	void kernel_execute_forwarder(const device_function& kernel,
								  const bool is_cooperative,
								  const bool wait_until_completion,
								  const uint3& global_size, const uint3& local_size,
								  kernel_completion_handler_f&& completion_handler,
								  const std::vector<device_function_arg>& args) const;
	
};

FLOOR_POP_WARNINGS()

} // namespace fl
