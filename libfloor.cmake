## libfloor dynamic library and executable CMake base configuration
# * define LIBFLOOR_USER when building a library/application using libfloor
# * define LIBFLOOR_LIBRARY when building a library using libfloor

## compile flags
target_compile_definitions(${PROJECT_NAME} PUBLIC _ENABLE_EXTENDED_ALIGNED_STORAGE)

if (MSVC)
	set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-imsvc ")
	if (CMAKE_BUILD_TYPE MATCHES "DEBUG" OR CMAKE_BUILD_TYPE MATCHES "Debug")
		target_compile_options(${PROJECT_NAME} PUBLIC -gcodeview /Zi -DFLOOR_DEBUG -D_DEBUG)
	endif ()
else ()
	set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-isystem ")
	if (CMAKE_BUILD_TYPE MATCHES "DEBUG" OR CMAKE_BUILD_TYPE MATCHES "Debug")
		target_compile_options(${PROJECT_NAME} PUBLIC -O0 -gdwarf-4 -DFLOOR_DEBUG -D_DEBUG -fno-omit-frame-pointer -fstandalone-debug)
	endif ()
endif (MSVC)

if (UNIX OR MINGW)
	set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-isystem ")
endif (UNIX OR MINGW)

if (WIN32)
	target_compile_definitions(${PROJECT_NAME} PUBLIC __WINDOWS__)
	if (MINGW)
		target_compile_definitions(${PROJECT_NAME} PUBLIC MINGW)
	endif (MINGW)
	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif (WIN32)

if (CMAKE_BUILD_TYPE MATCHES "RELEASE" OR CMAKE_BUILD_TYPE MATCHES "Release")
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:-Ofast>")
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:-msse4.1>")
endif ()
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:-fstrict-aliasing>")
if (UNIX OR MINGW)
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:-ffast-math>")
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:-funroll-loops>")
endif (UNIX OR MINGW)
if (MSVC)
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:/fp:fast>")
endif (MSVC)

if (WITH_ASAN)
	# enable asan
	target_compile_options(${PROJECT_NAME} PUBLIC -fsanitize=address -fsanitize-address-use-after-scope)
	target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
	# enable full backtraces everywhere
	target_compile_options(${PROJECT_NAME} PUBLIC -fno-omit-frame-pointer -fno-optimize-sibling-calls)
	# enable sanitizer recovery
	target_compile_options(${PROJECT_NAME} PUBLIC -fsanitize-recover=all)
endif (WITH_ASAN)

if (WITH_LIBCXX)
	# use libc++ as the STL
	target_compile_options(${PROJECT_NAME} PUBLIC -stdlib=libc++)
	target_link_options(${PROJECT_NAME} PUBLIC -stdlib=libc++)
	target_link_libraries(${PROJECT_NAME} PUBLIC c++abi pthread)
endif (WITH_LIBCXX)

## warnings
# let's start with everything
target_compile_options(${PROJECT_NAME} PUBLIC -Weverything)
# in case we're using warning options that aren't supported by other clang versions
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-unknown-warning-option)
# remove std compat warnings (c++20 with gnu and clang extensions is required)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c++98-compat -Wno-c++98-compat-pedantic)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c++11-compat -Wno-c++11-compat-pedantic)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c++14-compat -Wno-c++14-compat-pedantic)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c++17-compat -Wno-c++17-compat-pedantic -Wno-c++17-extensions)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c++2a-compat -Wno-c++2a-compat-pedantic -Wno-c++2a-extensions)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c99-extensions -Wno-c11-extensions)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-gnu -Wno-gcc-compat)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-nullability-extension)
# don't be too pedantic
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-header-hygiene -Wno-documentation -Wno-documentation-unknown-command
	-Wno-old-style-cast -Wno-global-constructors -Wno-exit-time-destructors -Wno-reserved-id-macro
	-Wno-reserved-identifier -Wno-date-time)
# suppress warnings in system headers
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-system-headers)
# these two are only useful in certain situations, but are quite noisy
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-packed -Wno-padded)
# this conflicts with the other switch/case warning
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-switch-enum)
# quite useful feature/extension
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-nested-anon-types)
# this should be taken care of in a different way
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-partial-availability)
# enable thread-safety warnings
target_compile_options(${PROJECT_NAME} PUBLIC
	-Wthread-safety -Wthread-safety-negative -Wthread-safety-beta -Wthread-safety-verbose)
# ignore "explicit move to avoid copying on older compilers" warning
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-return-std-move-in-c++11)
# ignore unsafe pointer/buffer access warnings
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-unsafe-buffer-usage)
# diagnostics
target_compile_options(${PROJECT_NAME} PUBLIC -fmacro-backtrace-limit=0)

## output postfix
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "d")

# on MinGW, ignore casing warnings (there is an issue where this warnings triggers even if the casing is correct)
if (MINGW)
	target_compile_options(${PROJECT_NAME} PUBLIC "-Wno-nonportable-system-include-path")
endif (MINGW)

# MinGW: use lld
if (MINGW)
	target_link_options(${PROJECT_NAME} PRIVATE -fuse-ld=lld)
endif (MINGW)

# MinGW: export everything (import lib flag "-Wl,--out-implib" is already added by CMake)
if (MINGW)
	target_link_options(${PROJECT_NAME} PRIVATE -Wl,--export-all-symbols)
endif (MINGW)

# UNIX/Linux: must export symbols to the dynamic symbol table
if (UNIX)
	target_link_options(${PROJECT_NAME} PRIVATE -Wl,--export-dynamic)
endif (UNIX)

## use console subsystem (MSVC/MINGW)
if (MSVC)
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
	target_compile_definitions(${PROJECT_NAME} PRIVATE _CONSOLE)
endif (MSVC)
if (MINGW)
	target_link_options(${PROJECT_NAME} PRIVATE -Wl,--subsystem,console)
	target_compile_definitions(${PROJECT_NAME} PRIVATE _CONSOLE)
endif (MINGW)

## dependencies/libraries/packages
find_package(SDL2 CONFIG REQUIRED)
if (LIBFLOOR_LIBRARY)
	if (MSVC)
		target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)
	else ()
		target_link_libraries(${PROJECT_NAME} PRIVATE SDL2)
	endif (MSVC)
else ()
	if (MSVC)
		target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main)
	else ()
		target_link_libraries(${PROJECT_NAME} PRIVATE SDL2 SDL2main)
		if (MINGW)
			# MinGW: use the normal main instead of the SDL one
			target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED)
		endif (MINGW)
	endif ()
endif (LIBFLOOR_LIBRARY)

find_package(OpenGL REQUIRED)
find_path(OPENGL_ARB_INCLUDE_DIR GL/glcorearb.h PATH_SUFFIXES mesa)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${OPENGL_ARB_INCLUDE_DIR})
if (WIN32)
	target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL32)
else ()
	target_link_libraries(${PROJECT_NAME} PRIVATE GL)
endif (WIN32)

find_package(OpenCL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCL_LIBRARIES})
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${OpenCL_INCLUDE_DIRS})

find_package(OpenSSL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)

find_path(ASIO_INCLUDE_DIR asio.hpp)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${ASIO_INCLUDE_DIR})

find_package(OpenAL CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenAL::OpenAL)

if (MSVC)
	target_link_directories(${PROJECT_NAME} PRIVATE $ENV{VK_SDK_PATH}/Lib)
	target_link_libraries(${PROJECT_NAME} PRIVATE vulkan-1)
endif (MSVC)
if (MINGW)
	find_package(Vulkan)
	if (Vulkan_FOUND)
		target_link_options(${PROJECT_NAME} PRIVATE "${Vulkan_LIBRARIES}")
	else ()
		# fall back to VK_SDK_PATH
		target_link_options(${PROJECT_NAME} PRIVATE "$ENV{VK_SDK_PATH}/Lib/vulkan-1.lib")
	endif (Vulkan_FOUND)
endif (MINGW)
if (UNIX)
	target_link_options(${PROJECT_NAME} PRIVATE -lvulkan)
endif (UNIX)
if (Vulkan_FOUND)
	target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${Vulkan_INCLUDE_DIRS})
else ()
	target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE $ENV{VK_SDK_PATH}/include)
endif (Vulkan_FOUND)

find_path(OPENVR_INCLUDE_DIR openvr.h PATH_SUFFIXES openvr)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${OPENVR_INCLUDE_DIR})
if (MSVC)
	target_link_directories(${PROJECT_NAME} PRIVATE ${OPENVR_INCLUDE_DIR}/../lib)
endif (MSVC)
target_link_libraries(${PROJECT_NAME} PRIVATE openvr_api)

find_package(OpenXR REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenXR::openxr_loader)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${OpenXR_INCLUDE_DIR})

if (UNIX)
	target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
endif (UNIX)

## add libfloor
if (LIBFLOOR_USER)
	if (WIN32)
		target_include_directories(${PROJECT_NAME} PRIVATE $ENV{ProgramW6432}/floor/include)
	else ()
		target_include_directories(${PROJECT_NAME} PRIVATE /opt/floor/include)
	endif (WIN32)
	if (MSVC)
		target_link_directories(${PROJECT_NAME} PRIVATE $ENV{ProgramW6432}/floor/lib)
		target_link_libraries(${PROJECT_NAME} PRIVATE debug libfloord optimized libfloor)
	endif (MSVC)
	if (MINGW)
		if (CMAKE_BUILD_TYPE MATCHES "DEBUG" OR CMAKE_BUILD_TYPE MATCHES "Debug")
			target_link_options(${PROJECT_NAME} PRIVATE "$ENV{ProgramW6432}/floor/lib/libfloord.dll.a")
		else ()
			target_link_options(${PROJECT_NAME} PRIVATE "$ENV{ProgramW6432}/floor/lib/libfloor.dll.a")
		endif ()
	endif (MINGW)
	if (UNIX)
		target_link_directories(${PROJECT_NAME} PRIVATE /opt/floor/lib)
		if (CMAKE_BUILD_TYPE MATCHES "DEBUG" OR CMAKE_BUILD_TYPE MATCHES "Debug")
			target_link_options(${PROJECT_NAME} PRIVATE -lfloord)
		else ()
			target_link_options(${PROJECT_NAME} PRIVATE -lfloor)
		endif ()
	endif (UNIX)
endif (LIBFLOOR_USER)
