# CMake project for libfloor
cmake_minimum_required(VERSION 3.21)
# NOTE: "lib" prefix is automatically added by CMake when not targeting VS/CMake
# also: can't use "if (MSVC)" before project(...), so use VCPKG_TARGET_TRIPLET instead
if (VCPKG_TARGET_TRIPLET)
	project(libfloor)
else (VCPKG_TARGET_TRIPLET)
	project(floor)
endif (VCPKG_TARGET_TRIPLET)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

## build options
option(BUILD_SHARED_LIBS "build libfloor as a shared library (otherwise builds as a static library)" ON)
option(WITH_ASAN "build with address sanitizer" OFF)
option(WITH_LIBCXX "build with libc++" OFF)

if (BUILD_SHARED_LIBS)
	message(">> building libfloor as a shared library")
else ()
	message(">> building libfloor as a static library")
endif (BUILD_SHARED_LIBS)

## compile flags
add_compile_definitions(FLOOR_DLL_EXPORT)
add_compile_options(-march=corei7-avx -mf16c)

if (WIN32 AND BUILD_SHARED_LIBS)
	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif (WIN32 AND BUILD_SHARED_LIBS)

## build output
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

if (BUILD_SHARED_LIBS)
	set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> cruvST <TARGET> <LINK_FLAGS> <OBJECTS>")
	set(CMAKE_CXX_ARCHIVE_APPEND "<CMAKE_AR> ruvST <TARGET> <LINK_FLAGS> <OBJECTS>")
endif (BUILD_SHARED_LIBS)

## source files
include_directories("../")
add_library(${PROJECT_NAME}
	compute/argument_buffer.cpp
	compute/argument_buffer.hpp
	compute/compute_buffer.cpp
	compute/compute_buffer.hpp
	compute/compute_common.hpp
	compute/compute_context.cpp
	compute/compute_context.hpp
	compute/compute_device.cpp
	compute/compute_device.hpp
	compute/compute_fence.cpp
	compute/compute_fence.hpp
	compute/compute_image.cpp
	compute/compute_image.hpp
	compute/compute_kernel.cpp
	compute/compute_kernel.hpp
	compute/compute_kernel_arg.hpp
	compute/compute_memory.cpp
	compute/compute_memory.hpp
	compute/compute_memory_flags.hpp
	compute/compute_program.cpp
	compute/compute_program.hpp
	compute/compute_queue.cpp
	compute/compute_queue.hpp
	compute/indirect_command.cpp
	compute/indirect_command.hpp
	compute/llvm_toolchain.cpp
	compute/llvm_toolchain.hpp
	compute/soft_printf.hpp
	compute/spirv_handler.cpp
	compute/spirv_handler.hpp
	compute/universal_binary.cpp
	compute/universal_binary.hpp
	compute/cuda/cuda_api.cpp
	compute/cuda/cuda_api.hpp
	compute/cuda/cuda_argument_buffer.cpp
	compute/cuda/cuda_argument_buffer.hpp
	compute/cuda/cuda_buffer.cpp
	compute/cuda/cuda_buffer.hpp
	compute/cuda/cuda_common.hpp
	compute/cuda/cuda_compute.cpp
	compute/cuda/cuda_compute.hpp
	compute/cuda/cuda_device.cpp
	compute/cuda/cuda_device.hpp
	compute/cuda/cuda_image.cpp
	compute/cuda/cuda_image.hpp
	compute/cuda/cuda_internal_api.hpp
	compute/cuda/cuda_kernel.cpp
	compute/cuda/cuda_kernel.hpp
	compute/cuda/cuda_program.cpp
	compute/cuda/cuda_program.hpp
	compute/cuda/cuda_queue.cpp
	compute/cuda/cuda_queue.hpp
	compute/device/atomic_compat.hpp
	compute/device/atomic_fallback.hpp
	compute/device/common.hpp
	compute/device/compute_algorithm.hpp
	compute/device/cuda.hpp
	compute/device/cuda_atomic.hpp
	compute/device/cuda_coop.hpp
	compute/device/cuda_id.hpp
	compute/device/cuda_image.hpp
	compute/device/cuda_pre.hpp
	compute/device/cuda_sampler.hpp
	compute/device/device_info.hpp
	compute/device/host.hpp
	compute/device/host_atomic.hpp
	compute/device/host_id.hpp
	compute/device/host_image.hpp
	compute/device/host_limits.hpp
	compute/device/host_post.hpp
	compute/device/host_pre.hpp
	compute/device/image.hpp
	compute/device/image_types.hpp
	compute/device/logger.hpp
	compute/device/metal.hpp
	compute/device/metal_atomic.hpp
	compute/device/metal_image.hpp
	compute/device/metal_post.hpp
	compute/device/metal_pre.hpp
	compute/device/mip_map_minify.hpp
	compute/device/opaque_image.hpp
	compute/device/opaque_image_map.hpp
	compute/device/opencl.hpp
	compute/device/opencl_atomic.hpp
	compute/device/opencl_image.hpp
	compute/device/opencl_pre.hpp
	compute/device/sampler.hpp
	compute/device/soft_printf.hpp
	compute/device/vulkan_image.hpp
	compute/device/vulkan_post.hpp
	compute/device/vulkan_pre.hpp
	compute/host/elf_binary.cpp
	compute/host/elf_binary.hpp
	compute/host/host_argument_buffer.cpp
	compute/host/host_argument_buffer.hpp
	compute/host/host_buffer.cpp
	compute/host/host_buffer.hpp
	compute/host/host_common.hpp
	compute/host/host_compute.cpp
	compute/host/host_compute.hpp
	compute/host/host_device.cpp
	compute/host/host_device.hpp
	compute/host/host_device_builtins.cpp
	compute/host/host_device_builtins.hpp
	compute/host/host_image.cpp
	compute/host/host_image.hpp
	compute/host/host_kernel.cpp
	compute/host/host_kernel.hpp
	compute/host/host_program.cpp
	compute/host/host_program.hpp
	compute/host/host_queue.cpp
	compute/host/host_queue.hpp
	compute/metal/metal_args.hpp
	compute/metal/metal_argument_buffer.hpp
	compute/metal/metal_argument_buffer.mm
	compute/metal/metal_buffer.hpp
	compute/metal/metal_buffer.mm
	compute/metal/metal_common.hpp
	compute/metal/metal_compute.hpp
	compute/metal/metal_compute.mm
	compute/metal/metal_device.cpp
	compute/metal/metal_device.hpp
	compute/metal/metal_image.hpp
	compute/metal/metal_image.mm
	compute/metal/metal_kernel.hpp
	compute/metal/metal_kernel.mm
	compute/metal/metal_program.hpp
	compute/metal/metal_program.mm
	compute/metal/metal_queue.hpp
	compute/metal/metal_queue.mm
	compute/opencl/opencl_buffer.cpp
	compute/opencl/opencl_buffer.hpp
	compute/opencl/opencl_common.hpp
	compute/opencl/opencl_compute.cpp
	compute/opencl/opencl_compute.hpp
	compute/opencl/opencl_device.cpp
	compute/opencl/opencl_device.hpp
	compute/opencl/opencl_image.cpp
	compute/opencl/opencl_image.hpp
	compute/opencl/opencl_kernel.cpp
	compute/opencl/opencl_kernel.hpp
	compute/opencl/opencl_program.cpp
	compute/opencl/opencl_program.hpp
	compute/opencl/opencl_queue.cpp
	compute/opencl/opencl_queue.hpp
	compute/vulkan/vulkan_args.hpp
	compute/vulkan/vulkan_argument_buffer.cpp
	compute/vulkan/vulkan_argument_buffer.hpp
	compute/vulkan/vulkan_buffer.cpp
	compute/vulkan/vulkan_buffer.hpp
	compute/vulkan/vulkan_common.cpp
	compute/vulkan/vulkan_common.hpp
	compute/vulkan/vulkan_compute.cpp
	compute/vulkan/vulkan_compute.hpp
	compute/vulkan/vulkan_descriptor_set.cpp
	compute/vulkan/vulkan_descriptor_set.hpp
	compute/vulkan/vulkan_device.cpp
	compute/vulkan/vulkan_device.hpp
	compute/vulkan/vulkan_disassembly.cpp
	compute/vulkan/vulkan_disassembly.hpp
	compute/vulkan/vulkan_encoder.hpp
	compute/vulkan/vulkan_fence.cpp
	compute/vulkan/vulkan_fence.hpp
	compute/vulkan/vulkan_image.cpp
	compute/vulkan/vulkan_image.hpp
	compute/vulkan/vulkan_indirect_command.cpp
	compute/vulkan/vulkan_indirect_command.hpp
	compute/vulkan/vulkan_kernel.cpp
	compute/vulkan/vulkan_kernel.hpp
	compute/vulkan/vulkan_memory.cpp
	compute/vulkan/vulkan_memory.hpp
	compute/vulkan/vulkan_program.cpp
	compute/vulkan/vulkan_program.hpp
	compute/vulkan/vulkan_queue.cpp
	compute/vulkan/vulkan_queue.hpp
	compute/vulkan/vulkan_semaphore.cpp
	compute/vulkan/vulkan_semaphore.hpp
	constexpr/const_array.hpp
	constexpr/const_math.hpp
	constexpr/const_string.hpp
	constexpr/ext_traits.hpp
	constexpr/sha_256.hpp
	constexpr/soft_f16.cpp
	constexpr/soft_f16.hpp
	graphics/graphics_renderer.cpp
	graphics/graphics_renderer.hpp
	graphics/graphics_pass.cpp
	graphics/graphics_pass.hpp
	graphics/graphics_pipeline.cpp
	graphics/graphics_pipeline.hpp
	graphics/graphics_index_type.hpp
	graphics/graphics_vertex_format.hpp
	graphics/metal/metal_renderer.mm
	graphics/metal/metal_renderer.hpp
	graphics/metal/metal_pass.mm
	graphics/metal/metal_pass.hpp
	graphics/metal/metal_pipeline.mm
	graphics/metal/metal_pipeline.hpp
	graphics/metal/metal_shader.mm
	graphics/metal/metal_shader.hpp
	graphics/vulkan/vulkan_renderer.cpp
	graphics/vulkan/vulkan_renderer.hpp
	graphics/vulkan/vulkan_pass.cpp
	graphics/vulkan/vulkan_pass.hpp
	graphics/vulkan/vulkan_pipeline.cpp
	graphics/vulkan/vulkan_pipeline.hpp
	graphics/vulkan/vulkan_shader.cpp
	graphics/vulkan/vulkan_shader.hpp
	core/aligned_ptr.hpp
	core/bcm.cpp
	core/bcm.hpp
	core/core.cpp
	core/core.hpp
	core/cpp_ext.hpp
	core/cpp_headers.hpp
	core/enum_helpers.hpp
	core/essentials.hpp
	core/event.cpp
	core/event.hpp
	core/event_objects.hpp
	core/file_io.cpp
	core/file_io.hpp
	core/flat_map.hpp
	core/json.cpp
	core/json.hpp
	core/logger.cpp
	core/logger.hpp
	core/option_handler.hpp
	core/platform.hpp
	core/platform_windows.hpp
	core/serializer.cpp
	core/serializer.hpp
	core/serializer_storage.hpp
	core/sig_handler.cpp
	core/sig_handler.hpp
	core/timer.hpp
	core/unicode.cpp
	core/unicode.hpp
	core/util.cpp
	core/util.hpp
	floor/build_version.hpp
	floor/floor.cpp
	floor/floor.hpp
	floor/floor_conf.hpp
	floor/floor_version.hpp
	lang/grammar.cpp
	lang/grammar.hpp
	lang/lang_context.hpp
	lang/lexer.cpp
	lang/lexer.hpp
	lang/source_types.cpp
	lang/source_types.hpp
	math/bbox.hpp
	math/constants.hpp
	math/matrix4.cpp
	math/matrix4.hpp
	math/quaternion.cpp
	math/quaternion.hpp
	math/dual_quaternion.cpp
	math/dual_quaternion.hpp
	math/ray.hpp
	math/rt_math.hpp
	math/vector.cpp
	math/vector.hpp
	math/vector_1d.cpp
	math/vector_2d.cpp
	math/vector_3d.cpp
	math/vector_4d.cpp
	math/vector_helper.hpp
	math/vector_lib.cpp
	math/vector_lib.hpp
	math/vector_lib_checks.hpp
	math/vector_ops.hpp
	math/vector_ops_cleanup.hpp
	threading/atomics.hpp
	threading/atomic_shared_ptr.hpp
	threading/atomic_spin_lock.hpp
	threading/task.hpp
	threading/thread_base.cpp
	threading/thread_base.hpp
	threading/thread_safety.hpp
	vr/openvr_context.cpp
	vr/openvr_context.hpp
	vr/openxr_common.hpp
	vr/openxr_context.cpp
	vr/openxr_context.hpp
	vr/openxr_input.cpp
	vr/vr_context.cpp
	vr/vr_context.hpp)

# include base configuration
set(LIBFLOOR_LIBRARY 1)
include(libfloor.cmake)
