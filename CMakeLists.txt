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
option(BUILD_REPRODUCIBLE "perform a reproducible build" OFF)
option(WITH_ASAN "build with address sanitizer" OFF)
option(WITH_TSAN "build with thread sanitizer" OFF)
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
include_directories("include" "src")
add_library(${PROJECT_NAME}
	include/floor/build_version.hpp
	include/floor/floor_conf.hpp
	include/floor/floor_version.hpp
	include/floor/floor.hpp
	include/floor/constexpr/const_array.hpp
	include/floor/constexpr/const_math.hpp
	include/floor/constexpr/const_string.hpp
	include/floor/constexpr/ext_traits.hpp
	include/floor/constexpr/sha_256.hpp
	include/floor/constexpr/soft_f16.hpp
	include/floor/core/aligned_ptr.hpp
	include/floor/core/bcm.hpp
	include/floor/core/core.hpp
	include/floor/core/cpp_ext.hpp
	include/floor/core/enum_helpers.hpp
	include/floor/core/essentials.hpp
	include/floor/core/event_objects.hpp
	include/floor/core/event.hpp
	include/floor/core/file_io.hpp
	include/floor/core/flat_map.hpp
	include/floor/core/hdr_metadata.hpp
	include/floor/core/json.hpp
	include/floor/core/logger.hpp
	include/floor/core/option_handler.hpp
	include/floor/core/platform_windows.hpp
	include/floor/core/platform.hpp
	include/floor/core/serializer_storage.hpp
	include/floor/core/serializer.hpp
	include/floor/core/sig_handler.hpp
	include/floor/core/timer.hpp
	include/floor/core/unicode.hpp
	include/floor/device/argument_buffer.hpp
	include/floor/device/backend/algorithm.hpp
	include/floor/device/backend/atomic_compat_post.hpp
	include/floor/device/backend/atomic_compat.hpp
	include/floor/device/backend/atomic_fallback.hpp
	include/floor/device/backend/common.hpp
	include/floor/device/backend/cuda_atomic.hpp
	include/floor/device/backend/cuda_coop.hpp
	include/floor/device/backend/cuda_group.hpp
	include/floor/device/backend/cuda_id.hpp
	include/floor/device/backend/cuda_image.hpp
	include/floor/device/backend/cuda_pre.hpp
	include/floor/device/backend/cuda_sampler.hpp
	include/floor/device/backend/cuda.hpp
	include/floor/device/backend/device_info.hpp
	include/floor/device/backend/group.hpp
	include/floor/device/backend/host_atomic.hpp
	include/floor/device/backend/host_id.hpp
	include/floor/device/backend/host_image.hpp
	include/floor/device/backend/host_limits.hpp
	include/floor/device/backend/host_post.hpp
	include/floor/device/backend/host_pre.hpp
	include/floor/device/backend/host.hpp
	include/floor/device/backend/image_types.hpp
	include/floor/device/backend/image.hpp
	include/floor/device/backend/logger.hpp
	include/floor/device/backend/metal_atomic.hpp
	include/floor/device/backend/metal_group.hpp
	include/floor/device/backend/metal_image.hpp
	include/floor/device/backend/metal_post.hpp
	include/floor/device/backend/metal_pre.hpp
	include/floor/device/backend/metal.hpp
	include/floor/device/backend/mip_map_minify.hpp
	include/floor/device/backend/opaque_image_map.hpp
	include/floor/device/backend/opaque_image.hpp
	include/floor/device/backend/opencl_atomic.hpp
	include/floor/device/backend/opencl_common.hpp
	include/floor/device/backend/opencl_image.hpp
	include/floor/device/backend/opencl_pre.hpp
	include/floor/device/backend/opencl.hpp
	include/floor/device/backend/sampler.hpp
	include/floor/device/backend/soft_pack.hpp
	include/floor/device/backend/soft_printf.hpp
	include/floor/device/backend/tessellation.hpp
	include/floor/device/backend/vulkan_group.hpp
	include/floor/device/backend/vulkan_image.hpp
	include/floor/device/backend/vulkan_post.hpp
	include/floor/device/backend/vulkan_pre.hpp
	include/floor/device/backend/vulkan.hpp
	include/floor/device/cuda/cuda_api.hpp
	include/floor/device/cuda/cuda_argument_buffer.hpp
	include/floor/device/cuda/cuda_buffer.hpp
	include/floor/device/cuda/cuda_common.hpp
	include/floor/device/cuda/cuda_context.hpp
	include/floor/device/cuda/cuda_device.hpp
	include/floor/device/cuda/cuda_function.hpp
	include/floor/device/cuda/cuda_image.hpp
	include/floor/device/cuda/cuda_internal_api.hpp
	include/floor/device/cuda/cuda_program.hpp
	include/floor/device/cuda/cuda_queue.hpp
	include/floor/device/device_buffer.hpp
	include/floor/device/device_common.hpp
	include/floor/device/device_context.hpp
	include/floor/device/device_fence.hpp
	include/floor/device/device_function_arg.hpp
	include/floor/device/device_function.hpp
	include/floor/device/device_image.hpp
	include/floor/device/device_memory_flags.hpp
	include/floor/device/device_memory.hpp
	include/floor/device/device_program.hpp
	include/floor/device/device_queue.hpp
	include/floor/device/device.hpp
	include/floor/device/graphics_index_type.hpp
	include/floor/device/graphics_pass.hpp
	include/floor/device/graphics_pipeline.hpp
	include/floor/device/graphics_renderer.hpp
	include/floor/device/graphics_vertex_format.hpp
	include/floor/device/host/elf_binary.hpp
	include/floor/device/host/host_argument_buffer.hpp
	include/floor/device/host/host_buffer.hpp
	include/floor/device/host/host_common.hpp
	include/floor/device/host/host_context.hpp
	include/floor/device/host/host_device_builtins.hpp
	include/floor/device/host/host_device.hpp
	include/floor/device/host/host_function.hpp
	include/floor/device/host/host_image.hpp
	include/floor/device/host/host_program.hpp
	include/floor/device/host/host_queue.hpp
	include/floor/device/indirect_command.hpp
	include/floor/device/metal/metal_args.hpp
	include/floor/device/metal/metal_argument_buffer.hpp
	include/floor/device/metal/metal_buffer.hpp
	include/floor/device/metal/metal_common.hpp
	include/floor/device/metal/metal_context.hpp
	include/floor/device/metal/metal_device_query.hpp
	include/floor/device/metal/metal_device.hpp
	include/floor/device/metal/metal_fence.hpp
	include/floor/device/metal/metal_function.hpp
	include/floor/device/metal/metal_image.hpp
	include/floor/device/metal/metal_indirect_command.hpp
	include/floor/device/metal/metal_pass.hpp
	include/floor/device/metal/metal_pipeline.hpp
	include/floor/device/metal/metal_program.hpp
	include/floor/device/metal/metal_queue.hpp
	include/floor/device/metal/metal_renderer.hpp
	include/floor/device/metal/metal_resource_tracking.hpp
	include/floor/device/metal/metal_shader.hpp
	include/floor/device/opencl/opencl_buffer.hpp
	include/floor/device/opencl/opencl_common.hpp
	include/floor/device/opencl/opencl_context.hpp
	include/floor/device/opencl/opencl_device.hpp
	include/floor/device/opencl/opencl_function.hpp
	include/floor/device/opencl/opencl_image.hpp
	include/floor/device/opencl/opencl_program.hpp
	include/floor/device/opencl/opencl_queue.hpp
	include/floor/device/soft_printf.hpp
	include/floor/device/spirv_handler.hpp
	include/floor/device/toolchain.hpp
	include/floor/device/universal_binary.hpp
	include/floor/device/vulkan/vulkan_argument_buffer.hpp
	include/floor/device/vulkan/vulkan_buffer.hpp
	include/floor/device/vulkan/vulkan_common.hpp
	include/floor/device/vulkan/vulkan_context.hpp
	include/floor/device/vulkan/vulkan_device.hpp
	include/floor/device/vulkan/vulkan_fence.hpp
	include/floor/device/vulkan/vulkan_function.hpp
	include/floor/device/vulkan/vulkan_fwd.hpp
	include/floor/device/vulkan/vulkan_image.hpp
	include/floor/device/vulkan/vulkan_indirect_command.hpp
	include/floor/device/vulkan/vulkan_memory.hpp
	include/floor/device/vulkan/vulkan_pass.hpp
	include/floor/device/vulkan/vulkan_pipeline.hpp
	include/floor/device/vulkan/vulkan_program.hpp
	include/floor/device/vulkan/vulkan_queue.hpp
	include/floor/device/vulkan/vulkan_renderer.hpp
	include/floor/device/vulkan/vulkan_semaphore.hpp
	include/floor/device/vulkan/vulkan_shader.hpp
	include/floor/lang/grammar.hpp
	include/floor/lang/lang_context.hpp
	include/floor/lang/lexer.hpp
	include/floor/lang/source_types.hpp
	include/floor/math/bbox.hpp
	include/floor/math/constants.hpp
	include/floor/math/dual_quaternion.hpp
	include/floor/math/matrix4.hpp
	include/floor/math/quaternion.hpp
	include/floor/math/ray.hpp
	include/floor/math/rt_math.hpp
	include/floor/math/vector_helper.hpp
	include/floor/math/vector_lib_checks.hpp
	include/floor/math/vector_lib.hpp
	include/floor/math/vector_ops_cleanup.hpp
	include/floor/math/vector_ops.hpp
	include/floor/math/vector.hpp
	include/floor/threading/atomic_shared_ptr.hpp
	include/floor/threading/atomic_spin_lock.hpp
	include/floor/threading/atomics.hpp
	include/floor/threading/safe_resource_container.hpp
	include/floor/threading/task.hpp
	include/floor/threading/thread_base.hpp
	include/floor/threading/thread_helpers.hpp
	include/floor/threading/thread_safety.hpp
	include/floor/vr/openvr_context.hpp
	include/floor/vr/openxr_common.hpp
	include/floor/vr/openxr_context.hpp
	include/floor/vr/vr_context.hpp
	
	src/constexpr/soft_f16.cpp
	src/core/bcm.cpp
	src/core/core.cpp
	src/core/event.cpp
	src/core/file_io.cpp
	src/core/json.cpp
	src/core/logger.cpp
	src/core/serializer.cpp
	src/core/sig_handler.cpp
	src/core/unicode.cpp
	src/device/argument_buffer.cpp
	src/device/cuda/cuda_api.cpp
	src/device/cuda/cuda_argument_buffer.cpp
	src/device/cuda/cuda_buffer.cpp
	src/device/cuda/cuda_context.cpp
	src/device/cuda/cuda_device.cpp
	src/device/cuda/cuda_function.cpp
	src/device/cuda/cuda_image.cpp
	src/device/cuda/cuda_program.cpp
	src/device/cuda/cuda_queue.cpp
	src/device/device_buffer.cpp
	src/device/device_context.cpp
	src/device/device_fence.cpp
	src/device/device_function.cpp
	src/device/device_image.cpp
	src/device/device_memory.cpp
	src/device/device_program.cpp
	src/device/device_queue.cpp
	src/device/device.cpp
	src/device/graphics_pass.cpp
	src/device/graphics_pipeline.cpp
	src/device/graphics_renderer.cpp
	src/device/host/elf_binary.cpp
	src/device/host/host_argument_buffer.cpp
	src/device/host/host_buffer.cpp
	src/device/host/host_context.cpp
	src/device/host/host_device_builtins.cpp
	src/device/host/host_device.cpp
	src/device/host/host_function.cpp
	src/device/host/host_image.cpp
	src/device/host/host_program.cpp
	src/device/host/host_queue.cpp
	src/device/indirect_command.cpp
	src/device/metal/metal_argument_buffer.mm
	src/device/metal/metal_buffer.mm
	src/device/metal/metal_context.mm
	src/device/metal/metal_device_query.mm
	src/device/metal/metal_device.cpp
	src/device/metal/metal_fence.mm
	src/device/metal/metal_function.mm
	src/device/metal/metal_image.mm
	src/device/metal/metal_indirect_command.mm
	src/device/metal/metal_pass.mm
	src/device/metal/metal_pipeline.mm
	src/device/metal/metal_program.mm
	src/device/metal/metal_queue.mm
	src/device/metal/metal_renderer.mm
	src/device/metal/metal_shader.mm
	src/device/opencl/opencl_buffer.cpp
	src/device/opencl/opencl_context.cpp
	src/device/opencl/opencl_device.cpp
	src/device/opencl/opencl_function.cpp
	src/device/opencl/opencl_image.cpp
	src/device/opencl/opencl_program.cpp
	src/device/opencl/opencl_queue.cpp
	src/device/spirv_handler.cpp
	src/device/toolchain.cpp
	src/device/universal_binary.cpp
	src/device/vulkan/internal/vulkan_args.hpp
	src/device/vulkan/internal/vulkan_conversion.hpp
	src/device/vulkan/internal/vulkan_debug.cpp
	src/device/vulkan/internal/vulkan_debug.hpp
	src/device/vulkan/internal/vulkan_descriptor_set.cpp
	src/device/vulkan/internal/vulkan_descriptor_set.hpp
	src/device/vulkan/internal/vulkan_disassembly.cpp
	src/device/vulkan/internal/vulkan_disassembly.hpp
	src/device/vulkan/internal/vulkan_drawable.hpp
	src/device/vulkan/internal/vulkan_encoder.hpp
	src/device/vulkan/internal/vulkan_function_entry.hpp
	src/device/vulkan/internal/vulkan_headers.hpp
	src/device/vulkan/internal/vulkan_heap.cpp
	src/device/vulkan/internal/vulkan_heap.hpp
	src/device/vulkan/internal/vulkan_image_internal.cpp
	src/device/vulkan/internal/vulkan_image_internal.hpp
	src/device/vulkan/vulkan_argument_buffer.cpp
	src/device/vulkan/vulkan_buffer.cpp
	src/device/vulkan/vulkan_common.cpp
	src/device/vulkan/vulkan_context.cpp
	src/device/vulkan/vulkan_device.cpp
	src/device/vulkan/vulkan_fence.cpp
	src/device/vulkan/vulkan_function.cpp
	src/device/vulkan/vulkan_image.cpp
	src/device/vulkan/vulkan_indirect_command.cpp
	src/device/vulkan/vulkan_memory.cpp
	src/device/vulkan/vulkan_pass.cpp
	src/device/vulkan/vulkan_pipeline.cpp
	src/device/vulkan/vulkan_program.cpp
	src/device/vulkan/vulkan_queue.cpp
	src/device/vulkan/vulkan_renderer.cpp
	src/device/vulkan/vulkan_semaphore.cpp
	src/device/vulkan/vulkan_shader.cpp
	src/floor/floor.cpp
	src/lang/grammar.cpp
	src/lang/lexer.cpp
	src/lang/source_types.cpp
	src/math/dual_quaternion.cpp
	src/math/matrix4.cpp
	src/math/quaternion.cpp
	src/math/vector_1d.cpp
	src/math/vector_2d.cpp
	src/math/vector_3d.cpp
	src/math/vector_4d.cpp
	src/math/vector_lib.cpp
	src/math/vector.cpp
	src/threading/thread_base.cpp
	src/threading/thread_helpers.cpp
	src/vr/internal/openxr_internal.hpp
	src/vr/openvr_context.cpp
	src/vr/openxr_context.cpp
	src/vr/openxr_input.cpp
	src/vr/vr_context.cpp
)

# include base configuration
set(LIBFLOOR_LIBRARY 1)
include(include/floor/libfloor.cmake)
