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

#if !defined(FLOOR_NO_CUDA)

#include <floor/core/enum_helpers.hpp>
#include <floor/math/vector_lib.hpp>
#include <floor/core/essentials.hpp>

namespace fl {

#if defined(__WINDOWS__)
#define CU_API __stdcall
#else
#define CU_API
#endif

enum class CU_RESULT : uint32_t {
	SUCCESS = 0,
	INVALID_VALUE = 1,
	OUT_OF_MEMORY = 2,
	NOT_INITIALIZED = 3,
	DEINITIALIZED = 4,
	PROFILER_DISABLED = 5,
	PROFILER_NOT_INITIALIZED = 6,
	PROFILER_ALREADY_STARTED = 7,
	PROFILER_ALREADY_STOPPED = 8,
	STUB_LIBRARY = 34,
	CALL_REQUIRES_NEWER_DRIVER = 36,
	DEVICE_UNAVAILABLE = 46,
	NO_DEVICE = 100,
	INVALID_DEVICE = 101,
	DEVICE_NOT_LICENSED = 102,
	INVALID_IMAGE = 200,
	INVALID_CONTEXT = 201,
	CONTEXT_ALREADY_CURRENT = 202,
	ERROR_MAP_FAILED = 205,
	UNMAP_FAILED = 206,
	ARRAY_IS_MAPPED = 207,
	ALREADY_MAPPED = 208,
	NO_BINARY_FOR_GPU = 209,
	ALREADY_ACQUIRED = 210,
	NOT_MAPPED = 211,
	NOT_MAPPED_AS_ARRAY = 212,
	NOT_MAPPED_AS_POINTER = 213,
	ECC_UNCORRECTABLE = 214,
	UNSUPPORTED_LIMIT = 215,
	CONTEXT_ALREADY_IN_USE = 216,
	PEER_ACCESS_UNSUPPORTED = 217,
	INVALID_PTX = 218,
	INVALID_GRAPHICS_CONTEXT = 219,
	NVLINK_UNCORRECTABLE = 220,
	JIT_COMPILER_NOT_FOUND = 221,
	UNSUPPORTED_PTX_VERSION = 222,
	JIT_COMPILATION_DISABLED = 223,
	UNSUPPORTED_EXEC_AFFINITY = 224,
	UNSUPPORTED_DEVSIDE_SYNC = 225,
	CONTAINED = 226,
	INVALID_SOURCE = 300,
	FILE_NOT_FOUND = 301,
	SHARED_OBJECT_SYMBOL_NOT_FOUND = 302,
	SHARED_OBJECT_INIT_FAILED = 303,
	OPERATING_SYSTEM = 304,
	INVALID_HANDLE = 400,
	ILLEGAL_STATE = 401,
	LOSSY_QUERY = 402,
	NOT_FOUND = 500,
	NOT_READY = 600,
	ILLEGAL_ADDRESS = 700,
	LAUNCH_OUT_OF_RESOURCES = 701,
	LAUNCH_TIMEOUT = 702,
	LAUNCH_INCOMPATIBLE_TEXTURING = 703,
	PEER_ACCESS_ALREADY_ENABLED = 704,
	PEER_ACCESS_NOT_ENABLED = 705,
	PRIMARY_CONTEXT_ACTIVE = 708,
	CONTEXT_IS_DESTROYED = 709,
	ASSERT = 710,
	TOO_MANY_PEERS = 711,
	HOST_MEMORY_ALREADY_REGISTERED = 712,
	HOST_MEMORY_NOT_REGISTERED = 713,
	HARDWARE_STACK_ERROR = 714,
	ILLEGAL_INSTRUCTION = 715,
	MISALIGNED_ADDRESS = 716,
	INVALID_ADDRESS_SPACE = 717,
	INVALID_PC = 718,
	LAUNCH_FAILED = 719,
	COOPERATIVE_LAUNCH_TOO_LARGE = 720,
	TENSOR_MEMORY_LEAK = 721,
	NOT_PERMITTED = 800,
	NOT_SUPPORTED = 801,
	SYSTEM_NOT_READY = 802,
	SYSTEM_DRIVER_MISMATCH = 803,
	COMPAT_NOT_SUPPORTED_ON_DEVICE = 804,
	MPS_CONNECTION_FAILED = 805,
	MPS_RPC_FAILURE = 806,
	MPS_SERVER_NOT_READY = 807,
	MPS_MAX_CLIENTS_REACHED = 808,
	MPS_MAX_CONNECTIONS_REACHED = 809,
	MPS_CLIENT_TERMINATED = 810,
	CDP_NOT_SUPPORTED = 811,
	CDP_VERSION_MISMATCH = 812,
	STREAM_CAPTURE_UNSUPPORTED = 900,
	STREAM_CAPTURE_INVALIDATED = 901,
	STREAM_CAPTURE_MERGE = 902,
	STREAM_CAPTURE_UNMATCHED = 903,
	STREAM_CAPTURE_UNJOINED = 904,
	STREAM_CAPTURE_ISOLATION = 905,
	STREAM_CAPTURE_IMPLICIT = 906,
	CAPTURED_EVENT = 907,
	STREAM_CAPTURE_WRONG_THREAD = 908,
	TIMEOUT = 909,
	GRAPH_EXEC_UPDATE_FAILURE = 910,
	EXTERNAL_DEVICE = 911,
	INVALID_CLUSTER_SIZE = 912,
	FUNCTION_NOT_LOADED = 913,
	INVALID_RESOURCE_TYPE = 914,
	INVALID_RESOURCE_CONFIGURATION = 915,
	KEY_ROTATION = 916,
	UNKNOWN = 999
};
enum class CU_DEVICE_ATTRIBUTE : uint32_t {
	MAX_THREADS_PER_BLOCK = 1,
	MAX_BLOCK_DIM_X = 2,
	MAX_BLOCK_DIM_Y = 3,
	MAX_BLOCK_DIM_Z = 4,
	MAX_GRID_DIM_X = 5,
	MAX_GRID_DIM_Y = 6,
	MAX_GRID_DIM_Z = 7,
	MAX_SHARED_MEMORY_PER_BLOCK = 8,
	SHARED_MEMORY_PER_BLOCK = 8,
	TOTAL_CONSTANT_MEMORY = 9,
	WARP_SIZE = 10,
	MAX_PITCH = 11,
	MAX_REGISTERS_PER_BLOCK = 12,
	REGISTERS_PER_BLOCK = 12,
	CLOCK_RATE = 13,
	TEXTURE_ALIGNMENT = 14,
	GPU_OVERLAP = 15,
	MULTIPROCESSOR_COUNT = 16,
	KERNEL_EXEC_TIMEOUT = 17,
	INTEGRATED = 18,
	CAN_MAP_HOST_MEMORY = 19,
	COMPUTE_MODE = 20,
	MAXIMUM_TEXTURE1D_WIDTH = 21,
	MAXIMUM_TEXTURE2D_WIDTH = 22,
	MAXIMUM_TEXTURE2D_HEIGHT = 23,
	MAXIMUM_TEXTURE3D_WIDTH = 24,
	MAXIMUM_TEXTURE3D_HEIGHT = 25,
	MAXIMUM_TEXTURE3D_DEPTH = 26,
	MAXIMUM_TEXTURE2D_LAYERED_WIDTH = 27,
	MAXIMUM_TEXTURE2D_LAYERED_HEIGHT = 28,
	MAXIMUM_TEXTURE2D_LAYERED_LAYERS = 29,
	MAXIMUM_TEXTURE2D_ARRAY_WIDTH = 27,
	MAXIMUM_TEXTURE2D_ARRAY_HEIGHT = 28,
	MAXIMUM_TEXTURE2D_ARRAY_NUMSLICES = 29,
	SURFACE_ALIGNMENT = 30,
	CONCURRENT_KERNELS = 31,
	ECC_ENABLED = 32,
	PCI_BUS_ID = 33,
	PCI_DEVICE_ID = 34,
	TCC_DRIVER = 35,
	MEMORY_CLOCK_RATE = 36,
	GLOBAL_MEMORY_BUS_WIDTH = 37,
	L2_CACHE_SIZE = 38,
	MAX_THREADS_PER_MULTIPROCESSOR = 39,
	ASYNC_ENGINE_COUNT = 40,
	UNIFIED_ADDRESSING = 41,
	MAXIMUM_TEXTURE1D_LAYERED_WIDTH = 42,
	MAXIMUM_TEXTURE1D_LAYERED_LAYERS = 43,
	CAN_TEX2D_GATHER = 44,
	MAXIMUM_TEXTURE2D_GATHER_WIDTH = 45,
	MAXIMUM_TEXTURE2D_GATHER_HEIGHT = 46,
	MAXIMUM_TEXTURE3D_WIDTH_ALTERNATE = 47,
	MAXIMUM_TEXTURE3D_HEIGHT_ALTERNATE = 48,
	MAXIMUM_TEXTURE3D_DEPTH_ALTERNATE = 49,
	PCI_DOMAIN_ID = 50,
	TEXTURE_PITCH_ALIGNMENT = 51,
	MAXIMUM_TEXTURECUBEMAP_WIDTH = 52,
	MAXIMUM_TEXTURECUBEMAP_LAYERED_WIDTH = 53,
	MAXIMUM_TEXTURECUBEMAP_LAYERED_LAYERS = 54,
	MAXIMUM_SURFACE1D_WIDTH = 55,
	MAXIMUM_SURFACE2D_WIDTH = 56,
	MAXIMUM_SURFACE2D_HEIGHT = 57,
	MAXIMUM_SURFACE3D_WIDTH = 58,
	MAXIMUM_SURFACE3D_HEIGHT = 59,
	MAXIMUM_SURFACE3D_DEPTH = 60,
	MAXIMUM_SURFACE1D_LAYERED_WIDTH = 61,
	MAXIMUM_SURFACE1D_LAYERED_LAYERS = 62,
	MAXIMUM_SURFACE2D_LAYERED_WIDTH = 63,
	MAXIMUM_SURFACE2D_LAYERED_HEIGHT = 64,
	MAXIMUM_SURFACE2D_LAYERED_LAYERS = 65,
	MAXIMUM_SURFACECUBEMAP_WIDTH = 66,
	MAXIMUM_SURFACECUBEMAP_LAYERED_WIDTH = 67,
	MAXIMUM_SURFACECUBEMAP_LAYERED_LAYERS = 68,
	MAXIMUM_TEXTURE1D_LINEAR_WIDTH = 69,
	MAXIMUM_TEXTURE2D_LINEAR_WIDTH = 70,
	MAXIMUM_TEXTURE2D_LINEAR_HEIGHT = 71,
	MAXIMUM_TEXTURE2D_LINEAR_PITCH = 72,
	MAXIMUM_TEXTURE2D_MIPMAPPED_WIDTH = 73,
	MAXIMUM_TEXTURE2D_MIPMAPPED_HEIGHT = 74,
	COMPUTE_CAPABILITY_MAJOR = 75,
	COMPUTE_CAPABILITY_MINOR = 76,
	MAXIMUM_TEXTURE1D_MIPMAPPED_WIDTH = 77,
	STREAM_PRIORITIES_SUPPORTED = 78,
	GLOBAL_L1_CACHE_SUPPORTED = 79,
	LOCAL_L1_CACHE_SUPPORTED = 80,
	MAX_SHARED_MEMORY_PER_MULTIPROCESSOR = 81,
	MAX_REGISTERS_PER_MULTIPROCESSOR = 82,
	MANAGED_MEMORY = 83,
	MULTI_GPU_BOARD = 84,
	MULTI_GPU_BOARD_GROUP_ID = 85,
	HOST_NATIVE_ATOMIC_SUPPORTED = 86,
	SINGLE_TO_DOUBLE_PRECISION_PERF_RATIO = 87,
	PAGEABLE_MEMORY_ACCESS = 88,
	CONCURRENT_MANAGED_ACCESS = 89,
	COMPUTE_PREEMPTION_SUPPORTED = 90,
	CAN_USE_HOST_POINTER_FOR_REGISTERED_MEM = 91,
	CAN_USE_STREAM_MEM_OPS_V1 = 92,
	CAN_USE_64_BIT_STREAM_MEM_OPS_V1 = 93,
	CAN_USE_STREAM_WAIT_VALUE_NOR_V1 = 94,
	COOPERATIVE_LAUNCH_SUPPORTED = 95,
	COOPERATIVE_MULTI_DEVICE_LAUNCH_SUPPORTED = 96,
	MAX_SHARED_MEMORY_PER_BLOCK_OPTIN = 97,
	CAN_FLUSH_REMOTE_WRITES = 98,
	HOST_REGISTER_SUPPORTED = 99,
	PAGEABLE_MEMORY_ACCESS_USES_HOST_PAGE_TABLES = 100,
	DIRECT_MANAGED_MEM_ACCESS_FROM_HOST = 101,
	VIRTUAL_ADDRESS_MANAGEMENT_SUPPORTED = 102,
	HANDLE_TYPE_POSIX_FILE_DESCRIPTOR_SUPPORTED = 103,
	HANDLE_TYPE_WIN32_HANDLE_SUPPORTED = 104,
	HANDLE_TYPE_WIN32_KMT_HANDLE_SUPPORTED = 105,
	MAX_BLOCKS_PER_MULTIPROCESSOR = 106,
	GENERIC_COMPRESSION_SUPPORTED = 107,
	MAX_PERSISTING_L2_CACHE_SIZE = 108,
	MAX_ACCESS_POLICY_WINDOW_SIZE = 109,
	GPU_DIRECT_RDMA_WITH_CUDA_VMM_SUPPORTED = 110,
	RESERVED_SHARED_MEMORY_PER_BLOCK = 111,
	SPARSE_CUDA_ARRAY_SUPPORTED = 112,
	READ_ONLY_HOST_REGISTER_SUPPORTED = 113,
	TIMELINE_SEMAPHORE_INTEROP_SUPPORTED = 114,
	MEMORY_POOLS_SUPPORTED = 115,
	GPU_DIRECT_RDMA_SUPPORTED = 116,
	GPU_DIRECT_RDMA_FLUSH_WRITES_OPTIONS = 117,
	GPU_DIRECT_RDMA_WRITES_ORDERING = 118,
	MEMPOOL_SUPPORTED_HANDLE_TYPES = 119,
	CLUSTER_LAUNCH = 120,
	DEFERRED_MAPPING_CUDA_ARRAY_SUPPORTED = 121,
	CAN_USE_64_BIT_STREAM_MEM_OPS_V2 = 122,
	CAN_USE_64_BIT_STREAM_MEM_OPS = CAN_USE_64_BIT_STREAM_MEM_OPS_V2,
	CAN_USE_STREAM_WAIT_VALUE_NOR_V2 = 123,
	CAN_USE_STREAM_WAIT_VALUE_NOR = CAN_USE_STREAM_WAIT_VALUE_NOR_V2,
	DMA_BUF_SUPPORTED = 124,
	IPC_EVENT_SUPPORTED = 125,
	MEM_SYNC_DOMAIN_COUNT = 126,
	TENSOR_MAP_ACCESS_SUPPORTED = 127,
	// CUDA 12.3+
	HANDLE_TYPE_FABRIC_SUPPORTED = 128,
	// CUDA 12.0+
	UNIFIED_FUNCTION_POINTERS = 129,
	// CUDA 12.2+
	NUMA_CONFIG = 130,
	NUMA_ID = 131,
	// CUDA 12.1+
	CU_DEVICE_ATTRIBUTE_MULTICAST_SUPPORTED = 132,
	// CUDA 12.3+
	MPS_ENABLED = 133,
	// CUDA 12.2+
	HOST_NUMA_ID = 134,
	// CUDA 12.5+
	D3D12_CIG_SUPPORTED = 135,
	// CUDA 12.8+
	MEM_DECOMPRESS_ALGORITHM_MASK = 136,
	MEM_DECOMPRESS_MAXIMUM_LENGTH = 137,
	// CUDA 12.9+
	VULKAN_CIG_SUPPORTED = 138,
	// CUDA 12.8+
	GPU_PCI_DEVICE_ID = 139,
	GPU_PCI_SUBSYSTEM_ID = 140,
	// CUDA 12.9+
	HOST_NUMA_VIRTUAL_MEMORY_MANAGEMENT_SUPPORTED = 141,
	HOST_NUMA_MEMORY_POOLS_SUPPORTED = 142,
	// CUDA 12.8+
	HOST_NUMA_MULTINODE_IPC_SUPPORTED = 143,
	// CUDA 13.0+
	HOST_MEMORY_POOLS_SUPPORTED = 144,
	HOST_VIRTUAL_MEMORY_MANAGEMENT_SUPPORTED = 145,
	HOST_ALLOC_DMA_BUF_SUPPORTED = 146,
	ONLY_PARTIAL_HOST_NATIVE_ATOMIC_SUPPORTED = 147,
};
enum class CU_FUNCTION_ATTRIBUTE : uint32_t {
	MAX_THREADS_PER_BLOCK = 0,
	//! NOTE: cl naming
	LOCAL_SIZE_BYTES = 1,
	CONST_SIZE_BYTES = 2,
	//! NOTE: cl naming
	PRIVATE_SIZE_BYTES = 3,
	NUM_REGISTERS = 4,
	PTX_VERSION = 5,
	BINARY_VERSION = 6,
	CACHE_MODE_CA = 7,
	//! NOTE: cl naming
	MAX_DYNAMIC_LOCAL_SIZE_BYTES = 8,
	//! NOTE: cl naming
	PREFERRED_LOCAL_MEMORY_CARVEOUT = 9,
	CLUSTER_SIZE_MUST_BE_SET = 10,
	REQUIRED_CLUSTER_WIDTH = 11,
	REQUIRED_CLUSTER_HEIGHT = 12,
	REQUIRED_CLUSTER_DEPTH = 13,
	NON_PORTABLE_CLUSTER_SIZE_ALLOWED = 14,
	CLUSTER_SCHEDULING_POLICY_PREFERENCE = 15,
};
enum class CU_JIT_OPTION : uint32_t {
	MAX_REGISTERS = 0,
	THREADS_PER_BLOCK = 1,
	WALL_TIME = 2,
	INFO_LOG_BUFFER = 3,
	INFO_LOG_BUFFER_SIZE_BYTES = 4,
	ERROR_LOG_BUFFER = 5,
	ERROR_LOG_BUFFER_SIZE_BYTES = 6,
	OPTIMIZATION_LEVEL = 7,
	TARGET_FROM_CUCONTEXT = 8,
	TARGET = 9,
	FALLBACK_STRATEGY = 10,
	GENERATE_DEBUG_INFO = 11,
	LOG_VERBOSE = 12,
	GENERATE_LINE_INFO = 13,
	CACHE_MODE = 14,
	NEW_SM3X_OPT = 15,
	FAST_COMPILE = 16,
	GLOBAL_SYMBOL_NAMES = 17,
	GLOBAL_SYMBOL_ADDRESSES = 18,
	GLOBAL_SYMBOL_COUNT = 19,
	LTO = 20,
	FTZ = 21,
	PREC_DIV = 22,
	PREC_SQRT = 23,
	FMA = 24,
	REFERENCED_KERNEL_NAMES = 25,
	REFERENCED_KERNEL_COUNT = 26,
	REFERENCED_VARIABLE_NAMES = 27,
	REFERENCED_VARIABLE_COUNT = 28,
	OPTIMIZE_UNUSED_DEVICE_VARIABLES = 29,
	POSITION_INDEPENDENT_CODE = 30,
	// CUDA 12.3+
	MIN_CTA_PER_SM = 31,
	// CUDA 12.4+
	MAX_THREADS_PER_BLOCK = 32,
	OVERRIDE_DIRECTIVE_VALUES = 33,
	// CUDA 13.0+
	SPLIT_COMPILE = 34,
};
enum class CU_JIT_INPUT_TYPE : uint32_t {
	CUBIN = 0,
	PTX = 1,
	FATBINARY = 2,
	OBJECT = 3,
	LIBRARY = 4,
	NVVM [[deprecated]] = 5,
};
enum class CU_LIMIT : uint32_t {
	STACK_SIZE = 0,
	PRINTF_FIFO_SIZE = 1,
	MALLOC_HEAP_SIZE = 2,
	DEV_RUNTIME_SYNC_DEPTH = 3,
	DEV_RUNTIME_PENDING_LAUNCH_COUNT = 4,
	MAX_L2_FETCH_GRANULARITY = 5,
	PERSISTING_L2_CACHE_SIZE = 6,
	// CUDA 12.5+
	CIG_SHMEM_SIZE = 7,
	CIG_ENABLED = 8,
	CIG_SHMEM_FALLBACK_ENABLED = 9,
};

enum class CU_ARRAY_FORMAT : uint32_t {
	UNSIGNED_INT8 = 0x01,
	UNSIGNED_INT16 = 0x02,
	UNSIGNED_INT32 = 0x03,
	SIGNED_INT8 = 0x08,
	SIGNED_INT16 = 0x09,
	SIGNED_INT32 = 0x0a,
	HALF = 0x10,
	FLOAT = 0x20,
	NV12 = 0xB0,
	UNORM_INT8X1 = 0xC0,
	UNORM_INT8X2 = 0xC1,
	UNORM_INT8X4 = 0xC2,
	UNORM_INT16X1 = 0xC3,
	UNORM_INT16X2 = 0xC4,
	UNORM_INT16X4 = 0xC5,
	SNORM_INT8X1 = 0xC6,
	SNORM_INT8X2 = 0xC7,
	SNORM_INT8X4 = 0xC8,
	SNORM_INT16X1 = 0xC9,
	SNORM_INT16X2 = 0xCA,
	SNORM_INT16X4 = 0xCB,
	BC1_UNORM = 0x91,
	BC1_UNORM_SRGB = 0x92,
	BC2_UNORM = 0x93,
	BC2_UNORM_SRGB = 0x94,
	BC3_UNORM = 0x95,
	BC3_UNORM_SRGB = 0x96,
	BC4_UNORM = 0x97,
	BC4_SNORM = 0x98,
	BC5_UNORM = 0x99,
	BC5_SNORM = 0x9A,
	BC6H_UF16 = 0x9B,
	BC6H_SF16 = 0x9C,
	BC7_UNORM = 0x9D,
	BC7_UNORM_SRGB = 0x9E,
	// CUDA 12.5+
	P010 = 0x9F,
	P016 = 0xA1,
	NV16 = 0xA2,
	P210 = 0xA3,
	P216 = 0xA4,
	YUY2 = 0xA5,
	Y210 = 0xA6,
	Y216 = 0xA7,
	AYUV = 0xA8,
	Y410 = 0xA9,
	Y416 = 0xB1,
	Y444_PLANAR8 = 0xB2,
	Y444_PLANAR10 = 0xB3,
	// CUDA 12.8+
	YUV444_8BIT_SEMI_PLANAR = 0xB4,
	YUV444_16BIT_SEMI_PLANAR = 0xB5,
	UNORM_INT_101010_2 = 0x50,
};

enum class CU_MEMORY_TYPE : uint32_t {
	HOST = 1,
	DEVICE = 2,
	ARRAY = 3,
	UNIFIED = 4
};

enum class CU_ADDRESS_MODE : uint32_t {
	WRAP = 0,
	CLAMP = 1,
	MIRROR = 2,
	BORDER = 3
};

enum class CU_FILTER_MODE : uint32_t {
	NEAREST = 0,
	LINEAR = 1
};

enum class CU_RESOURCE_TYPE : uint32_t {
	ARRAY = 0,
	MIP_MAPPED_ARRAY = 1,
	LINEAR = 2,
	PITCH_2D = 3
};

enum class CU_RESOURCE_VIEW_FORMAT : uint32_t {
	NONE = 0,
	UINT_1X8 = 1,
	UINT_2X8 = 2,
	UINT_4X8 = 3,
	SINT_1X8 = 4,
	SINT_2X8 = 5,
	SINT_4X8 = 6,
	UINT_1X16 = 7,
	UINT_2X16 = 8,
	UINT_4X16 = 9,
	SINT_1X16 = 10,
	SINT_2X16 = 11,
	SINT_4X16 = 12,
	UINT_1X32 = 13,
	UINT_2X32 = 14,
	UINT_4X32 = 15,
	SINT_1X32 = 16,
	SINT_2X32 = 17,
	SINT_4X32 = 18,
	FLOAT_1X16 = 19,
	FLOAT_2X16 = 20,
	FLOAT_4X16 = 21,
	FLOAT_1X32 = 22,
	FLOAT_2X32 = 23,
	FLOAT_4X32 = 24,
	UNSIGNED_BC1 = 25,
	UNSIGNED_BC2 = 26,
	UNSIGNED_BC3 = 27,
	UNSIGNED_BC4 = 28,
	SIGNED_BC4 = 29,
	UNSIGNED_BC5 = 30,
	SIGNED_BC5 = 31,
	UNSIGNED_BC6H = 32,
	SIGNED_BC6H = 33,
	UNSIGNED_BC7 = 34
};

enum class CU_MEM_HOST_ALLOC : uint32_t {
	PORTABLE = 1,
	DEVICE_MAP = 2,
	WRITE_COMBINED = 4
};
floor_global_enum_ext(CU_MEM_HOST_ALLOC)

enum class CU_MEM_HOST_REGISTER : uint32_t {
	PORTABLE = 1,
	DEVICE_MAP = 2,
	IO_MEMORY = 4,
	READ_ONLY = 8,
};
floor_global_enum_ext(CU_MEM_HOST_REGISTER)

enum class CU_GRAPHICS_REGISTER_FLAGS : uint32_t {
	NONE = 0,
	READ_ONLY = 1,
	WRITE_DISCARD = 2,
	SURFACE_LOAD_STORE = 4,
	TEXTURE_GATHER = 8
};
floor_global_enum_ext(CU_GRAPHICS_REGISTER_FLAGS)

enum class CU_CONTEXT_FLAGS : uint32_t {
	SCHEDULE_AUTO = 0,
	SCHEDULE_SPIN = 1,
	SCHEDULE_YIELD = 2,
	SCHEDULE_BLOCKING_SYNC = 4,
	MAP_HOST = 8,
	LMEM_RESIZE_TO_MAX = 16,
	// CUDA 12.1+
	COREDUMP_ENABLE = 32,
	USER_COREDUMP_ENABLE = 64,
	SYNC_MEMOPS = 128,
};
floor_global_enum_ext(CU_CONTEXT_FLAGS)

enum class CU_STREAM_FLAGS : uint32_t {
	DEFAULT = 0,
	NON_BLOCKING = 1
};

enum class CU_ARRAY_3D_FLAGS : uint32_t {
	NONE = 0,
	LAYERED = 1,
	SURFACE_LOAD_STORE = 2,
	CUBE_MAP = 4,
	TEXTURE_GATHER = 8,
	//! NOTE: unsupported for standalone CUDA use, required for Vulkan
	DEPTH_TEXTURE = 16,
	//! NOTE: unsupported for standalone CUDA use, required for Vulkan
	COLOR_ATTACHMENT = 32,
	SPARSE = 64,
	DEFERRED_MAPPING = 128,
	// CUDA 12.5+
	VIDEO_ENCODE_DECODE = 256,
};
floor_global_enum_ext(CU_ARRAY_3D_FLAGS)

enum class CU_TEXTURE_FLAGS : uint32_t {
	NONE = 0,
	READ_AS_INTEGER = 1,
	NORMALIZED_COORDINATES = 2,
	SRGB = 16,
	DISABLE_TRILINEAR_OPTIMIZATION = 32,
	SEAMLESS_CUBEMAP = 64,
};
floor_global_enum_ext(CU_TEXTURE_FLAGS)

enum class CU_EVENT_FLAGS : uint32_t {
	DEFAULT 		= 0u,
	BLOCKING_SYNC	= (1u << 0u),
	DISABLE_TIMING	= (1u << 1u),
	INTERPROCESS	= (1u << 2u),
};

enum class CU_MEM_DECOMPRESS_ALGORITHM : uint32_t {
	UNSUPPORTED	= 0u,
	DEFLATE		= (1u << 0u),
	SNAPPY		= (1u << 1u),
	LZ4			= (1u << 2u),
};
floor_global_enum_ext(CU_MEM_DECOMPRESS_ALGORITHM)

#define CU_LAUNCH_PARAM_BUFFER_POINTER ((void*)1)
#define CU_LAUNCH_PARAM_BUFFER_SIZE ((void*)2)
#define CU_LAUNCH_PARAM_END nullptr

// CUDA 12.9+
#define CU_LAUNCH_KERNEL_REQUIRED_BLOCK_DIM 1

// these are all external opaque types
struct _cu_context;
using cu_context = _cu_context*;
using const_cu_context = const _cu_context*;

struct _cu_texture_ref;
using cu_texture_ref = _cu_texture_ref*;
using const_cu_texture_ref = const _cu_texture_ref*;

struct _cu_array;
using cu_array = _cu_array*;
using const_cu_array = _cu_array*;

struct _cu_mip_mapped_array;
using cu_mip_mapped_array = _cu_mip_mapped_array*;
using const_cu_mip_mapped_array = const _cu_mip_mapped_array*;

struct _cu_stream;
using cu_stream = _cu_stream*;
using const_cu_stream = const _cu_stream*;

struct _cu_module;
using cu_module = _cu_module*;
using const_cu_module = const _cu_module*;

struct _cu_function;
using cu_function = _cu_function*;
using const_cu_function = const _cu_function*;

struct _cu_graphics_resource;
using cu_graphics_resource = _cu_graphics_resource*;
using const_cu_graphics_resource = const _cu_graphics_resource*;

struct _cu_link_state;
using cu_link_state = _cu_link_state*;
using const_cu_link_state = const _cu_link_state*;

struct _cu_event;
using cu_event = _cu_event*;
using const_cu_event = const _cu_event*;

// also: cu_graph_device_updatable_node
struct _cu_graph_device_node;
using cu_graph_device_node = _cu_graph_device_node*;
using const_cu_graph_device_node = const _cu_graph_device_node*;

using cu_device = int32_t;
using cu_device_ptr = size_t;
using cu_surf_object = uint64_t;
using cu_tex_object = uint64_t;
using cu_tex_only_object = uint32_t;
using cu_occupancy_b2d_size = size_t (CU_API *)(int32_t block_size);
using cu_stream_callback = void (CU_API *)(cu_stream stream, CU_RESULT result, void* user_data);

// structs that can actually be filled by the user
struct cu_array_3d_descriptor {
	size3 dim;
	CU_ARRAY_FORMAT format;
	uint32_t channel_count;
	CU_ARRAY_3D_FLAGS flags;
};

struct cu_memcpy_3d_descriptor {
	struct info {
		size_t x_in_bytes;
		size_t y;
		size_t z;
		size_t lod;
		CU_MEMORY_TYPE memory_type;
		const void* host_ptr;
		cu_device_ptr device_ptr;
		cu_array array;
		void* _reserved;
		size_t pitch;
		size_t height;
	};
	info src;
	info dst;
	
	size_t width_in_bytes;
	size_t height;
	size_t depth;
};

struct cu_resource_descriptor {
	CU_RESOURCE_TYPE type;
	
	union {
		struct {
			cu_array array;
		};
		struct {
			cu_mip_mapped_array mip_mapped_array;
		};
		struct {
			cu_device_ptr device_ptr;
			CU_ARRAY_FORMAT format;
			uint32_t channel_count;
			size_t size_in_bytes;
		} linear;
		struct {
			cu_device_ptr device_ptr;
			CU_ARRAY_FORMAT format;
			uint32_t channel_count;
			size_t width;
			size_t height;
			size_t pitch_in_bytes;
		} pitch_2d;
		struct {
			int32_t _reserved[32];
		};
	};
	
	uint32_t _flags { 0 }; // must always be zero
};

struct cu_resource_view_descriptor {
	CU_RESOURCE_VIEW_FORMAT format;
	size3 dim;
	uint32_t first_mip_map_level;
	uint32_t last_mip_map_level;
	uint32_t first_layer;
	uint32_t last_layer;
	uint32_t _reserved[16];
};

struct cu_texture_descriptor {
	CU_ADDRESS_MODE address_mode[3];
	CU_FILTER_MODE filter_mode;
	CU_TEXTURE_FLAGS flags;
	uint32_t max_anisotropy;
	CU_FILTER_MODE mip_map_filter_mode;
	float mip_map_level_bias;
	float min_mip_map_level_clamp;
	float max_mip_map_level_clamp;
	float4 _border_color;
	int32_t _reserved[12];
};

struct cu_launch_params {
	cu_function function;
	uint32_t grid_dim_x;
	uint32_t grid_dim_y;
	uint32_t grid_dim_z;
	uint32_t block_dim_x;
	uint32_t block_dim_y;
	uint32_t block_dim_z;
	uint32_t shared_mem_bytes;
	cu_stream stream;
	void** kernel_params;
};

struct cu_uuid {
	uint8_t bytes[16];
};

struct _cu_external_memory;
using cu_external_memory = _cu_external_memory*;
using const_cu_external_memory = const _cu_external_memory*;

struct _cu_external_semaphore;
using cu_external_semaphore = _cu_external_semaphore*;
using const_cu_external_semaphore = const _cu_external_semaphore*;

enum class CU_EXTERNAL_MEMORY_HANDLE_TYPE : uint32_t {
	OPAQUE_FD = 1,
	OPAQUE_WIN32 = 2,
	OPAQUE_WIN32_KMT = 3,
	D3D12_HEAP __attribute__((unavailable("unsupported"))) = 4,
	D3D12_RESOURCE __attribute__((unavailable("unsupported"))) = 5,
	D3D11_RESOURCE __attribute__((unavailable("unsupported"))) = 6,
	D3D11_RESOURCE_KMT __attribute__((unavailable("unsupported"))) = 7,
	NVSCIBUF = 8,
	// CUDA 13.0+
	DMABUF_FD = 9,
};

enum class CU_EXTERNAL_MEMORY_FLAGS : uint32_t {
	DEDICATED_MEMORY = 1,
};

enum class CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE : uint32_t {
	OPAQUE_FD = 1,
	OPAQUE_WIN32 = 2,
	OPAQUE_WIN32_KMT = 3,
	D3D12_FENCE __attribute__((unavailable("unsupported"))) = 4,
	D3D11_FENCE __attribute__((unavailable("unsupported"))) = 5,
	NVSCISYNC = 6,
	D3D11_KEYED_MUTEX __attribute__((unavailable("unsupported"))) = 7,
	D3D11_KEYED_MUTEX_KMT __attribute__((unavailable("unsupported"))) = 8,
	TIMELINE_SEMAPHORE_FD = 9,
	TIMELINE_SEMAPHORE_WIN32 = 10,
};

enum class CU_EXTERNAL_SEMAPHORE_FLAGS : uint32_t {
	SIGNAL_SKIP_NVSCIBUF_MEMSYNC = 1,
	WAIT_SKIP_NVSCIBUF_MEMSYNC = 2,
};

struct cu_external_memory_handle_descriptor {
	CU_EXTERNAL_MEMORY_HANDLE_TYPE type;
	union {
		int fd;
		struct {
			void* handle;
			const void* name;
		} win32;
		const void* nv_sci_buf_object;
	} handle;
	uint64_t size;
	uint32_t flags;
	uint32_t _reserved[16] { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
};

struct cu_external_memory_buffer_descriptor {
	uint64_t offset;
	uint64_t size;
	uint32_t flags;
	uint32_t _reserved[16] { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
};

struct cu_external_memory_mip_mapped_array_descriptor {
	uint64_t offset;
	cu_array_3d_descriptor array_desc;
	uint32_t num_levels;
	uint32_t _reserved[16] { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
};

struct cu_external_semaphore_handle_descriptor {
	CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE type;
	union {
		int fd;
		struct {
			void* handle;
			const void* name;
		} win32;
		const void* nv_sci_sync_object;
	} handle;
	uint32_t flags;
	uint32_t _reserved[16] { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
};

struct cu_external_semaphore_signal_parameters {
	struct {
		struct {
			uint64_t value;
		} fence;
		union {
			void* fence;
			uint64_t reserved;
		} nv_sci_sync;
		struct {
			uint64_t key;
		} keyed_mutex;
		uint32_t _reserved[12] { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	} params;
	uint32_t flags;
	uint32_t _reserved[16] { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
};

struct cu_external_semaphore_wait_parameters {
	struct {
		struct {
			uint64_t value;
		} fence;
		union {
			void* fence;
			uint64_t reserved;
		} nv_sci_sync;
		struct {
			uint64_t key;
			uint32_t timeout_ms;
		} keyed_mutex;
		uint32_t _reserved[10] { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	} params;
	uint32_t flags;
	uint32_t _reserved[16] { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
};

enum class CU_LAUNCH_ATTRIBUTE {
	IGNORE = 0,
	ACCESS_POLICY_WINDOW = 1,
	COOPERATIVE = 2,
	SYNCHRONIZATION_POLICY = 3,
	CLUSTER_DIMENSION = 4,
	CLUSTER_SCHEDULING_POLICY_PREFERENCE = 5,
	PROGRAMMATIC_STREAM_SERIALIZATION = 6,
	PROGRAMMATIC_EVENT = 7,
	PRIORITY = 8,
	MEM_SYNC_DOMAIN_MAP = 9,
	MEM_SYNC_DOMAIN = 10,
	// CUDA 12.8+
	PREFERRED_CLUSTER_DIMENSION = 11,
	// CUDA 12.3+
	LAUNCH_COMPLETION_EVENT = 12,
	// CUDA 12.4+
	DEVICE_UPDATABLE_KERNEL_NODE = 13,
	// CUDA 12.5+
	PREFERRED_SHARED_MEMORY_CARVEOUT = 14,
	// CUDA 13.0+
	NVLINK_UTIL_CENTRIC_SCHEDULING = 16,
};

enum class CU_ACCESS_PROPERTY {
	NORMAL = 0,
	STREAMING = 1,
	PERSISTING = 2,
};

struct cu_access_policy_window {
	void* base_ptr;
	size_t num_bytes;
	float hit_ratio;
	CU_ACCESS_PROPERTY hit_prop;
	CU_ACCESS_PROPERTY miss_prop;
};

enum class CU_SYNCHRONIZATION_POLICY {
	AUTO = 1,
	SPIN = 2,
	YIELD = 3,
	BLOCKING_SYNC = 4,
};

enum class CU_CLUSTER_SCHEDULING_POLICY {
	DEFAULT = 0,
	SPREAD = 1,
	LOAD_BALANCING = 2,
};

enum class CU_LAUNCH_MEM_SYNC_DOMAIN {
	DEFAULT = 0,
	REMOTE = 1,
};

struct cu_launch_mem_sync_domain_map {
	uint32_t default_domain;
	uint32_t remote_domain;
};

union cu_launch_attribute_value {
	char pad[64];
	cu_access_policy_window access_policy_window;
	int cooperative;
	CU_SYNCHRONIZATION_POLICY sync_policy;
	struct {
		uint32_t x;
		uint32_t y;
		uint32_t z;
	} cluster_dim;
	CU_CLUSTER_SCHEDULING_POLICY cluster_scheduling_policy_preference;
	int programmatic_stream_serialization_allowed;
	struct {
		cu_event event;
		int flags;
		int trigger_at_block_start;
	} programmatic_event;
	struct {
		cu_event event;
		int flags;
	} launch_completion_event;
	int priority;
	cu_launch_mem_sync_domain_map mem_sync_domain_map;
	CU_LAUNCH_MEM_SYNC_DOMAIN mem_sync_domain;
	struct {
		uint32_t x;
		uint32_t y;
		uint32_t z;
	} preferred_cluster_dim;
	struct {
		int device_updatable;
		cu_graph_device_node dev_node;
	} device_updatable_kernel_node;
	uint32_t shared_memory_carveout;
	// CUDA 13.0+
	uint32_t nvlink_util_centric_scheduling;
};

struct cu_launch_attribute {
	CU_LAUNCH_ATTRIBUTE type;
	char pad[8 - sizeof(CU_LAUNCH_ATTRIBUTE)];
	cu_launch_attribute_value value;
};

struct cu_launch_config {
	uint32_t grid_dim_x;
	uint32_t grid_dim_y;
	uint32_t grid_dim_z;
	uint32_t block_dim_x;
	uint32_t block_dim_y;
	uint32_t block_dim_z;
	uint32_t shared_memory_bytes;
	const_cu_stream stream;
	cu_launch_attribute* attrs;
	uint32_t num_attrs;
};

// CUDA 12.5+
enum class CU_EXECUTION_AFFINITY_TYPE : uint32_t {
	SM_COUNT = 0,
};

// CUDA 12.5+
struct cu_execution_affinity_sm_count {
	uint32_t sm_count;
};

// CUDA 12.5+
struct cu_execution_affinity_parameter {
	CU_EXECUTION_AFFINITY_TYPE type;
	union {
		cu_execution_affinity_sm_count sm_count;
	} param;
};

// CUDA 12.5+
enum class CU_CIG_DATA_TYPE : uint32_t {
	D3D12_COMMAND_QUEUE = 1,
	// CUDA 12.9+
	NV_BLOB = 2,
};

// CUDA 12.5+
struct cu_ctx_cig_parameter {
	CU_CIG_DATA_TYPE shared_data_type;
	void* shared_data;
};

// CUDA 12.5+
struct cu_ctx_create_parameters {
	cu_execution_affinity_parameter* execution_affinity_parameters;
	uint32_t num_execution_affinity_parameters;
	cu_ctx_cig_parameter* cig_parameter;
};

} // namespace fl

// internal API structs
#include <floor/device/cuda/cuda_internal_api.hpp>

namespace fl {

// actual CUDA API function pointers
struct cuda_api_ptrs {
	CU_API CU_RESULT (*array_3d_create)(cu_array* p_handle, const cu_array_3d_descriptor* p_allocate_array);
	CU_API CU_RESULT (*array_3d_get_descriptor)(cu_array_3d_descriptor* p_array_descriptor, cu_array h_array);
	CU_API CU_RESULT (*array_destroy)(cu_array h_array);
	CU_API CU_RESULT (*ctx_create)(cu_context* pctx, cu_execution_affinity_parameter* params_array, int num_params, CU_CONTEXT_FLAGS flags, cu_device dev);
	CU_API CU_RESULT (*ctx_create_v4)(cu_context* pctx, cu_ctx_create_parameters* create_params, CU_CONTEXT_FLAGS flags, cu_device dev);
	CU_API CU_RESULT (*ctx_get_limit)(size_t* pvalue, CU_LIMIT limit);
	CU_API CU_RESULT (*ctx_set_current)(cu_context ctx);
	CU_API CU_RESULT (*destroy_external_memory)(cu_external_memory ext_mem);
	CU_API CU_RESULT (*destroy_external_semaphore)(cu_external_semaphore ext_sem);
	CU_API CU_RESULT (*device_compute_capability)(int32_t* major, int32_t* minor, cu_device dev);
	CU_API CU_RESULT (*device_get)(cu_device* device, int32_t ordinal);
	CU_API CU_RESULT (*device_get_attribute)(int32_t* pi, CU_DEVICE_ATTRIBUTE attrib, cu_device dev);
	CU_API CU_RESULT (*device_get_count)(int32_t* count);
	CU_API CU_RESULT (*device_get_name)(char* name, int32_t len, cu_device dev);
	CU_API CU_RESULT (*device_get_uuid)(cu_uuid* uuid, cu_device dev);
	CU_API CU_RESULT (*device_total_mem)(size_t* bytes, cu_device dev);
	CU_API CU_RESULT (*driver_get_version)(int32_t* driver_version);
	CU_API CU_RESULT (*event_create)(cu_event* evt, CU_EVENT_FLAGS flags);
	CU_API CU_RESULT (*event_destroy)(cu_event evt);
	CU_API CU_RESULT (*event_elapsed_time)(float* milli_seconds, cu_event start_evt, cu_event end_evt);
	CU_API CU_RESULT (*event_record)(cu_event evt, const_cu_stream stream);
	CU_API CU_RESULT (*event_synchronize)(cu_event evt);
	CU_API CU_RESULT (*external_memory_get_mapped_buffer)(cu_device_ptr* dev_ptr, cu_external_memory ext_mem, const cu_external_memory_buffer_descriptor* buffer_desc);
	CU_API CU_RESULT (*external_memory_get_mapped_mip_mapped_array)(cu_mip_mapped_array* mip_map, cu_external_memory ext_mem, const cu_external_memory_mip_mapped_array_descriptor* mip_map_desc);
	CU_API CU_RESULT (*func_get_name)(const char** name, cu_function func);
	CU_API CU_RESULT (*function_get_attribute)(int32_t* ret, CU_FUNCTION_ATTRIBUTE attrib, cu_function hfunc);
	CU_API CU_RESULT (*func_load)(cu_function func);
	CU_API CU_RESULT (*get_error_name)(CU_RESULT error, const char** p_str);
	CU_API CU_RESULT (*get_error_string)(CU_RESULT error, const char** p_str);
	CU_API CU_RESULT (*graphics_map_resources)(uint32_t count, cu_graphics_resource* resources, const_cu_stream h_stream);
	CU_API CU_RESULT (*graphics_resource_get_mapped_mipmapped_array)(cu_mip_mapped_array* handle, cu_graphics_resource resource);
	CU_API CU_RESULT (*graphics_resource_get_mapped_pointer)(cu_device_ptr* p_dev_ptr, size_t* p_size, cu_graphics_resource resource);
	CU_API CU_RESULT (*graphics_sub_resource_get_mapped_array)(cu_array* p_array, cu_graphics_resource resource, uint32_t array_index, uint32_t mip_level);
	CU_API CU_RESULT (*graphics_unmap_resources)(uint32_t count, cu_graphics_resource* resources, const_cu_stream h_stream);
	CU_API CU_RESULT (*import_external_memory)(cu_external_memory* ext_mem_out, const cu_external_memory_handle_descriptor* mem_handle_desc);
	CU_API CU_RESULT (*import_external_semaphore)(cu_external_semaphore* ext_sem_out, const cu_external_semaphore_handle_descriptor* sem_handle_desc);
	CU_API CU_RESULT (*init)(uint32_t flags);
	CU_API CU_RESULT (*launch_kernel)(cu_function f, uint32_t grid_dim_x, uint32_t grid_dim_y, uint32_t grid_dim_z, uint32_t block_dim_x, uint32_t block_dim_y, uint32_t block_dim_z, uint32_t shared_mem_bytes, const_cu_stream h_stream, void** kernel_params, void** extra);
	CU_API CU_RESULT (*launch_kernel_ex)(const cu_launch_config* config, cu_function f, void** kernel_params, void** extra);
	CU_API CU_RESULT (*launch_cooperative_kernel)(cu_function f, uint32_t grid_dim_x, uint32_t grid_dim_y, uint32_t grid_dim_z, uint32_t block_dim_x, uint32_t block_dim_y, uint32_t block_dim_z, uint32_t shared_mem_bytes, const_cu_stream h_stream, void** kernel_params);
	CU_API CU_RESULT (*launch_cooperative_kernel_multi_device)(cu_launch_params* launch_params, uint32_t num_devices, uint32_t flags);
	CU_API CU_RESULT (*link_add_data)(cu_link_state state, CU_JIT_INPUT_TYPE type, const void* data, size_t size, const char* name, uint32_t num_options, const CU_JIT_OPTION* options, const void* const* option_values);
	CU_API CU_RESULT (*link_complete)(cu_link_state state, void** cubin_out, size_t* size_out);
	CU_API CU_RESULT (*link_create)(uint32_t num_options, const CU_JIT_OPTION* options, const void* const* option_values, cu_link_state* state_out);
	CU_API CU_RESULT (*link_destroy)(cu_link_state state);
	CU_API CU_RESULT (*mem_alloc)(cu_device_ptr* dptr, size_t bytesize);
	CU_API CU_RESULT (*mem_free)(cu_device_ptr dptr);
	CU_API CU_RESULT (*mem_free_host)(void* hptr);
	CU_API CU_RESULT (*mem_get_info)(size_t* free, size_t* total);
	CU_API CU_RESULT (*mem_host_alloc)(void** hptr, size_t bytesize);
	CU_API CU_RESULT (*mem_host_get_device_pointer)(cu_device_ptr* pdptr, void* p, uint32_t flags);
	CU_API CU_RESULT (*mem_host_register)(void* p, size_t bytesize, CU_MEM_HOST_REGISTER flags);
	CU_API CU_RESULT (*mem_host_unregister)(void* p);
	CU_API CU_RESULT (*memcpy_3d_async)(const cu_memcpy_3d_descriptor* p_copy, const_cu_stream h_stream);
	CU_API CU_RESULT (*memcpy_dtod_async)(cu_device_ptr dst_device, cu_device_ptr src_device, size_t byte_count, const_cu_stream h_stream);
	CU_API CU_RESULT (*memcpy_dtoh_async)(void* dst_host, cu_device_ptr src_device, size_t byte_count, const_cu_stream h_stream);
	CU_API CU_RESULT (*memcpy_htod_async)(cu_device_ptr dst_device, const void* src_host, size_t byte_count, const_cu_stream h_stream);
	CU_API CU_RESULT (*memset_d16_async)(cu_device_ptr dst_device, uint16_t us, size_t n, const_cu_stream h_stream);
	CU_API CU_RESULT (*memset_d32_async)(cu_device_ptr dst_device, uint32_t ui, size_t n, const_cu_stream h_stream);
	CU_API CU_RESULT (*memset_d8_async)(cu_device_ptr dst_device, unsigned char uc, size_t n, const_cu_stream h_stream);
	CU_API CU_RESULT (*mipmapped_array_create)(cu_mip_mapped_array* handle, const cu_array_3d_descriptor* desc, uint32_t num_mipmap_levels);
	CU_API CU_RESULT (*mipmapped_array_destroy)(cu_mip_mapped_array handle);
	CU_API CU_RESULT (*mipmapped_array_get_level)(cu_array* level_array, cu_mip_mapped_array mipmapped_array, uint32_t level);
	CU_API CU_RESULT (*module_enumerate_functions)(cu_function* functions, uint32_t function_count, cu_module mod);
	CU_API CU_RESULT (*module_get_function)(cu_function* hfunc, cu_module hmod, const char* name);
	CU_API CU_RESULT (*module_get_function_count)(uint32_t* count, cu_module mod);
	CU_API CU_RESULT (*module_load_data)(cu_module* mod, const void* image);
	CU_API CU_RESULT (*module_load_data_ex)(cu_module* mod, const void* image, uint32_t num_options, const CU_JIT_OPTION* options, const void* const* option_values);
	CU_API CU_RESULT (*occupancy_max_active_blocks_per_multiprocessor)(int32_t* num_blocks, cu_function func, int32_t block_size, size_t dynamic_s_mem_size);
	CU_API CU_RESULT (*occupancy_max_active_blocks_per_multiprocessor_with_flags)(int32_t* num_blocks, cu_function func, int32_t block_size, size_t dynamic_s_mem_size, uint32_t flags);
	CU_API CU_RESULT (*occupancy_max_potential_block_size)(int32_t* min_grid_size, int32_t* block_size, cu_function func, cu_occupancy_b2d_size block_size_to_dynamic_s_mem_size, size_t dynamic_s_mem_size, int32_t block_size_limit);
	CU_API CU_RESULT (*occupancy_max_potential_block_size_with_flags)(int32_t* min_grid_size, int32_t* block_size, cu_function func, cu_occupancy_b2d_size block_size_to_dynamic_s_mem_size, size_t dynamic_s_mem_size, int32_t block_size_limit, uint32_t flags);
	CU_API CU_RESULT (*signal_external_semaphore_async)(const cu_external_semaphore* ext_sem_array, const cu_external_semaphore_signal_parameters* params_array, const uint32_t num_ext_sems, cu_stream stream);
	CU_API CU_RESULT (*stream_add_callback)(const_cu_stream h_stream, cu_stream_callback callback, void* user_data, uint32_t flags);
	CU_API CU_RESULT (*stream_create)(cu_stream* ph_stream, CU_STREAM_FLAGS flags);
	CU_API CU_RESULT (*stream_synchronize)(const_cu_stream h_stream);
	CU_API CU_RESULT (*surf_object_create)(cu_surf_object* p_surf_object, const cu_resource_descriptor* p_res_desc);
	CU_API CU_RESULT (*surf_object_destroy)(cu_surf_object surf_object);
	CU_API CU_RESULT (*tex_object_create)(cu_tex_object* p_tex_object, const cu_resource_descriptor* p_res_desc, const cu_texture_descriptor* p_tex_desc, const cu_resource_view_descriptor* p_res_view_desc);
	CU_API CU_RESULT (*tex_object_destroy)(cu_tex_object tex_object);
	CU_API CU_RESULT (*tex_object_get_resource_desc)(cu_resource_descriptor* desc, cu_tex_object tex_object);
	CU_API CU_RESULT (*wait_external_semaphore_async)(const cu_external_semaphore* ext_sem_array, const cu_external_semaphore_wait_parameters* params_array, const uint32_t num_ext_sems, cu_stream stream);
};
extern cuda_api_ptrs cuda_api;
extern bool cuda_api_init(const bool use_internal_api);

extern uint32_t cuda_device_sampler_func_offset;
extern uint32_t cuda_device_in_ctx_offset;
extern bool cuda_can_use_internal_api();

extern bool cuda_can_use_external_memory();

#define cu_array_3d_create cuda_api.array_3d_create
#define cu_array_3d_get_descriptor cuda_api.array_3d_get_descriptor
#define cu_array_destroy cuda_api.array_destroy
#define cu_ctx_create cuda_api.ctx_create
#define cu_ctx_create_v4 cuda_api.ctx_create_v4
#define cu_ctx_get_limit cuda_api.ctx_get_limit
#define cu_ctx_set_current cuda_api.ctx_set_current
#define cu_destroy_external_memory cuda_api.destroy_external_memory
#define cu_destroy_external_semaphore cuda_api.destroy_external_semaphore
#define cu_device_compute_capability cuda_api.device_compute_capability
#define cu_device_get cuda_api.device_get
#define cu_device_get_attribute cuda_api.device_get_attribute
#define cu_device_get_count cuda_api.device_get_count
#define cu_device_get_name cuda_api.device_get_name
#define cu_device_get_uuid cuda_api.device_get_uuid
#define cu_device_total_mem cuda_api.device_total_mem
#define cu_driver_get_version cuda_api.driver_get_version
#define cu_event_create cuda_api.event_create
#define cu_event_destroy cuda_api.event_destroy
#define cu_event_elapsed_time cuda_api.event_elapsed_time
#define cu_event_record cuda_api.event_record
#define cu_event_synchronize cuda_api.event_synchronize
#define cu_external_memory_get_mapped_buffer cuda_api.external_memory_get_mapped_buffer
#define cu_external_memory_get_mapped_mip_mapped_array cuda_api.external_memory_get_mapped_mip_mapped_array
#define cu_func_get_name cuda_api.func_get_name
#define cu_function_get_attribute cuda_api.function_get_attribute
#define cu_func_load cuda_api.func_load
#define cu_get_error_name cuda_api.get_error_name
#define cu_get_error_string cuda_api.get_error_string
#define cu_graphics_map_resources cuda_api.graphics_map_resources
#define cu_graphics_resource_get_mapped_mipmapped_array cuda_api.graphics_resource_get_mapped_mipmapped_array
#define cu_graphics_resource_get_mapped_pointer cuda_api.graphics_resource_get_mapped_pointer
#define cu_graphics_sub_resource_get_mapped_array cuda_api.graphics_sub_resource_get_mapped_array
#define cu_graphics_unmap_resources cuda_api.graphics_unmap_resources
#define cu_import_external_memory cuda_api.import_external_memory
#define cu_import_external_semaphore cuda_api.import_external_semaphore
#define cu_init cuda_api.init
#define cu_launch_kernel cuda_api.launch_kernel
#define cu_launch_kernel_ex cuda_api.launch_kernel_ex
#define cu_launch_cooperative_kernel cuda_api.launch_cooperative_kernel
#define cu_launch_cooperative_kernel_multi_device cuda_api.launch_cooperative_kernel_multi_device
#define cu_link_add_data cuda_api.link_add_data
#define cu_link_complete cuda_api.link_complete
#define cu_link_create cuda_api.link_create
#define cu_link_destroy cuda_api.link_destroy
#define cu_mem_alloc cuda_api.mem_alloc
#define cu_mem_free cuda_api.mem_free
#define cu_mem_free_host cuda_api.mem_free_host
#define cu_mem_get_info cuda_api.mem_get_info
#define cu_mem_host_alloc cuda_api.mem_host_alloc
#define cu_mem_host_get_device_pointer cuda_api.mem_host_get_device_pointer
#define cu_mem_host_register cuda_api.mem_host_register
#define cu_mem_host_unregister cuda_api.mem_host_unregister
#define cu_memcpy_3d cuda_api.memcpy_3d
#define cu_memcpy_3d_async cuda_api.memcpy_3d_async
#define cu_memcpy_dtod cuda_api.memcpy_dtod
#define cu_memcpy_dtod_async cuda_api.memcpy_dtod_async
#define cu_memcpy_dtoh cuda_api.memcpy_dtoh
#define cu_memcpy_dtoh_async cuda_api.memcpy_dtoh_async
#define cu_memcpy_htod cuda_api.memcpy_htod
#define cu_memcpy_htod_async cuda_api.memcpy_htod_async
#define cu_memset_d16 cuda_api.memset_d16
#define cu_memset_d32 cuda_api.memset_d32
#define cu_memset_d8 cuda_api.memset_d8
#define cu_memset_d16_async cuda_api.memset_d16_async
#define cu_memset_d32_async cuda_api.memset_d32_async
#define cu_memset_d8_async cuda_api.memset_d8_async
#define cu_mipmapped_array_create cuda_api.mipmapped_array_create
#define cu_mipmapped_array_destroy cuda_api.mipmapped_array_destroy
#define cu_mipmapped_array_get_level cuda_api.mipmapped_array_get_level
#define cu_module_enumerate_functions cuda_api.module_enumerate_functions
#define cu_module_get_function cuda_api.module_get_function
#define cu_module_get_function_count cuda_api.module_get_function_count
#define cu_module_load_data cuda_api.module_load_data
#define cu_module_load_data_ex cuda_api.module_load_data_ex
#define cu_occupancy_max_active_blocks_per_multiprocessor cuda_api.occupancy_max_active_blocks_per_multiprocessor
#define cu_occupancy_max_active_blocks_per_multiprocessor_with_flags cuda_api.occupancy_max_active_blocks_per_multiprocessor_with_flags
#define cu_occupancy_max_potential_block_size cuda_api.occupancy_max_potential_block_size
#define cu_occupancy_max_potential_block_size_with_flags cuda_api.occupancy_max_potential_block_size_with_flags
#define cu_signal_external_semaphore_async cuda_api.signal_external_semaphore_async
#define cu_stream_add_callback cuda_api.stream_add_callback
#define cu_stream_create cuda_api.stream_create
#define cu_stream_synchronize cuda_api.stream_synchronize
#define cu_surf_object_create cuda_api.surf_object_create
#define cu_surf_object_destroy cuda_api.surf_object_destroy
#define cu_tex_object_create cuda_api.tex_object_create
#define cu_tex_object_destroy cuda_api.tex_object_destroy
#define cu_tex_object_get_resource_desc cuda_api.tex_object_get_resource_desc
#define cu_wait_external_semaphore_async cuda_api.wait_external_semaphore_async

} // namespace fl

#endif
