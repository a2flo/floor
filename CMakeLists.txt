# VS/vcpkg/clang/CMake project for libfloor
cmake_minimum_required(VERSION 3.8)
project(libfloor)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

## compile flags
set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-imsvc ")
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
add_compile_definitions(__WINDOWS__)
add_compile_definitions(_ENABLE_EXTENDED_ALIGNED_STORAGE)
add_compile_definitions(FLOOR_DLL_EXPORT)
add_compile_options("$<$<CONFIG:DEBUG>:-DFLOOR_DEBUG>")
add_compile_options("$<$<CONFIG:DEBUG>:-gcodeview>")
add_compile_options("$<$<CONFIG:DEBUG>:/Zi>")

## warnings
# let's start with everything
add_compile_options(-Weverything)
# in case we're using warning options that aren't supported by other clang versions
add_compile_options(-Wno-unknown-warning-option)
# remove std compat warnings (c++17 with gnu and clang extensions is required)
add_compile_options(-Wno-c++98-compat -Wno-c++98-compat-pedantic)
add_compile_options(-Wno-c++11-compat -Wno-c++11-compat-pedantic)
add_compile_options(-Wno-c++14-compat -Wno-c++14-compat-pedantic)
add_compile_options(-Wno-c99-extensions -Wno-c11-extensions)
add_compile_options(-Wno-gnu -Wno-gcc-compat)
# don't be too pedantic
add_compile_options(-Wno-header-hygiene -Wno-documentation -Wno-documentation-unknown-command -Wno-old-style-cast)
add_compile_options(-Wno-global-constructors -Wno-exit-time-destructors -Wno-reserved-id-macro -Wno-date-time)
# suppress warnings in system headers
add_compile_options(-Wno-system-headers)
# these two are only useful in certain situations, but are quite noisy
add_compile_options(-Wno-packed -Wno-padded)
# this conflicts with the other switch/case warning
add_compile_options(-Wno-switch-enum)
# quite useful feature/extension
add_compile_options(-Wno-nested-anon-types)
# this should be taken care of in a different way
add_compile_options(-Wno-partial-availability)
# enable thread-safety warnings
add_compile_options(-Wthread-safety -Wthread-safety-negative -Wthread-safety-beta -Wthread-safety-verbose)
# ignore "explicit move to avoid copying on older compilers" warning
add_compile_options(-Wno-return-std-move-in-c++11)
# diagnostics
add_compile_options(-fmacro-backtrace-limit=0)

## build output
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

## source files
include_directories("../")
add_library(${PROJECT_NAME} SHARED
audio/audio_controller.cpp
audio/audio_controller.hpp
audio/audio_headers.cpp
audio/audio_headers.hpp
audio/audio_source.cpp
audio/audio_source.hpp
audio/audio_store.cpp
audio/audio_store.hpp
compute/compute_buffer.cpp
compute/compute_buffer.hpp
compute/compute_common.hpp
compute/compute_context.cpp
compute/compute_context.hpp
compute/compute_device.cpp
compute/compute_device.hpp
compute/compute_image.cpp
compute/compute_image.hpp
compute/compute_kernel.cpp
compute/compute_kernel.hpp
compute/compute_kernel_arg.hpp
compute/compute_memory.cpp
compute/compute_memory.hpp
compute/compute_program.cpp
compute/compute_program.hpp
compute/compute_queue.cpp
compute/compute_queue.hpp
compute/llvm_toolchain.cpp
compute/llvm_toolchain.hpp
compute/soft_printf.hpp
compute/spirv_handler.cpp
compute/spirv_handler.hpp
compute/universal_binary.cpp
compute/universal_binary.hpp
compute/cuda/cuda_api.cpp
compute/cuda/cuda_api.hpp
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
compute/host/host_buffer.cpp
compute/host/host_buffer.hpp
compute/host/host_common.hpp
compute/host/host_compute.cpp
compute/host/host_compute.hpp
compute/host/host_device.cpp
compute/host/host_device.hpp
compute/host/host_image.cpp
compute/host/host_image.hpp
compute/host/host_kernel.cpp
compute/host/host_kernel.hpp
compute/host/host_program.cpp
compute/host/host_program.hpp
compute/host/host_queue.cpp
compute/host/host_queue.hpp
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
compute/metal/metal_args.hpp
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
compute/vulkan/vulkan_buffer.cpp
compute/vulkan/vulkan_buffer.hpp
compute/vulkan/vulkan_common.hpp
compute/vulkan/vulkan_compute.cpp
compute/vulkan/vulkan_compute.hpp
compute/vulkan/vulkan_device.cpp
compute/vulkan/vulkan_device.hpp
compute/vulkan/vulkan_encoder.hpp
compute/vulkan/vulkan_image.cpp
compute/vulkan/vulkan_image.hpp
compute/vulkan/vulkan_kernel.cpp
compute/vulkan/vulkan_kernel.hpp
compute/vulkan/vulkan_memory.cpp
compute/vulkan/vulkan_memory.hpp
compute/vulkan/vulkan_program.cpp
compute/vulkan/vulkan_program.hpp
compute/vulkan/vulkan_queue.cpp
compute/vulkan/vulkan_queue.hpp
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
core/gl_shader.cpp
core/gl_shader.hpp
core/gl_support.cpp
core/gl_support.hpp
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
net/asio_error_handler.cpp
net/asio_error_handler.hpp
net/http_net.hpp
net/irc_net.hpp
net/net.hpp
net/net_protocol.hpp
net/net_tcp.hpp
threading/atomics.hpp
threading/atomic_shared_ptr.hpp
threading/atomic_spin_lock.hpp
threading/task.cpp
threading/task.hpp
threading/thread_base.cpp
threading/thread_base.hpp
threading/thread_safety.hpp
)

## output postfix
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "d")

## use console subsystem
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
add_compile_definitions(_CONSOLE)

## dependencies/libraries/packages
find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)

find_package(OpenGL REQUIRED)
find_path(OPENGL_ARB_INCLUDE_DIR GL/glcorearb.h)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${OPENGL_ARB_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL32)

find_package(OpenCL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCL_LIBRARIES})
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${OpenCL_INCLUDE_DIRS})

find_package(OpenSSL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)

find_path(ASIO_INCLUDE_DIR asio.hpp)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${ASIO_INCLUDE_DIR})

find_package(OpenAL CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenAL::OpenAL)

target_link_directories(${PROJECT_NAME} PRIVATE $ENV{VK_SDK_PATH}/Lib)
target_link_libraries(${PROJECT_NAME} PRIVATE vulkan-1)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE $ENV{VK_SDK_PATH}/include)
