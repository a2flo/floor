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
		target_compile_options(${PROJECT_NAME} PUBLIC -O0 -gdwarf-5 -DFLOOR_DEBUG -D_DEBUG -fno-omit-frame-pointer -fstandalone-debug)
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
endif (WIN32)

if (CMAKE_BUILD_TYPE MATCHES "RELEASE" OR CMAKE_BUILD_TYPE MATCHES "Release")
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:-O3>")
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:-msse4.1>")
endif ()
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:-fstrict-aliasing>")
if (MSVC)
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:/fp:fast>")
else ()
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:-ffast-math>")
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:-funroll-loops>")
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
# remove std compat warnings (c++23 with gnu and clang extensions is required)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c++98-compat -Wno-c++98-compat-pedantic)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c++11-compat -Wno-c++11-compat-pedantic)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c++14-compat -Wno-c++14-compat-pedantic)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c++17-compat -Wno-c++17-compat-pedantic -Wno-c++17-extensions)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c++20-compat -Wno-c++20-compat-pedantic -Wno-c++20-extensions)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c++23-compat -Wno-c++23-compat-pedantic -Wno-c++23-extensions)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c99-extensions -Wno-c11-extensions -Wno-c17-extensions -Wno-c23-extensions)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-gnu -Wno-gcc-compat)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-nullability-extension)
# don't be too pedantic
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-header-hygiene -Wno-documentation -Wno-documentation-unknown-command
	-Wno-old-style-cast -Wno-global-constructors -Wno-exit-time-destructors -Wno-reserved-id-macro
	-Wno-reserved-identifier -Wno-date-time -Wno-poison-system-directories)
# suppress warnings in system headers
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-system-headers)
# these two are only useful in certain situations, but are quite noisy
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-packed -Wno-padded)
# these conflict with the other switch/case warning
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-switch-enum -Wno-switch-default)
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
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-unsafe-buffer-usage -Wno-unsafe-buffer-usage-in-container)
# ignore UD NaN/infinity due to fast-math
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-nan-infinity-disabled)
# ignore warnings about missing designated initializer when they are default-initialized
# on clang < 19: disable missing field initializer warnings altogether
if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.0.0")
	target_compile_options(${PROJECT_NAME} PUBLIC -Wno-missing-field-initializers)
else ()
	target_compile_options(${PROJECT_NAME} PUBLIC -Wno-missing-designated-field-initializers)
endif ()
# ignore missing include directories, we may not always have all specified include dirs
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-missing-include-dirs)
# diagnostics
target_compile_options(${PROJECT_NAME} PUBLIC -fdiagnostics-show-note-include-stack -fmessage-length=0 -fmacro-backtrace-limit=0)
target_compile_options(${PROJECT_NAME} PUBLIC -fparse-all-comments -fno-elide-type -fdiagnostics-show-template-tree)

## output postfix
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "d")

# on MinGW, ignore casing warnings (there is an issue where this triggers warnings even if the casing is correct)
if (MINGW)
	target_compile_options(${PROJECT_NAME} PUBLIC -Wno-nonportable-system-include-path)
endif (MINGW)
# similarly, also do this with a MSVC toolchain, but do it for all non-system includes as well
if (MSVC)
	target_compile_options(${PROJECT_NAME} PUBLIC -Wno-nonportable-system-include-path)
	target_compile_options(${PROJECT_NAME} PUBLIC -Wno-nonportable-include-path)
endif (MSVC)

# MinGW: use lld
if (MINGW)
	target_link_options(${PROJECT_NAME} PRIVATE -fuse-ld=lld)
endif (MINGW)

if (BUILD_SHARED_LIBS)
	# MinGW: export everything (import lib flag "-Wl,--out-implib" is already added by CMake)
	if (MINGW)
		target_link_options(${PROJECT_NAME} PRIVATE -Wl,--export-all-symbols)
	endif (MINGW)

	# UNIX/Linux: must export symbols to the dynamic symbol table
	if (UNIX)
		target_link_options(${PROJECT_NAME} PRIVATE -Wl,--export-dynamic)
	endif (UNIX)
endif (BUILD_SHARED_LIBS)

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
find_package(SDL3 CONFIG REQUIRED)
if (MSVC)
	target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
else ()
	target_link_libraries(${PROJECT_NAME} PRIVATE SDL3)
	if (LIBFLOOR_LIBRARY AND MINGW)
		# MinGW: use the normal main instead of the SDL one
		target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED)
	endif (LIBFLOOR_LIBRARY AND MINGW)
endif (MSVC)

find_package(OpenCL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCL_LIBRARIES})
target_include_directories(${PROJECT_NAME} SYSTEM AFTER PRIVATE ${OpenCL_INCLUDE_DIRS})

# we only want Vulkan headers
find_package(VulkanHeaders)
if (VulkanHeaders_FOUND)
	target_include_directories(${PROJECT_NAME} SYSTEM AFTER PRIVATE ${Vulkan_INCLUDE_DIR})
else ()
	target_include_directories(${PROJECT_NAME} SYSTEM AFTER PRIVATE $ENV{VK_SDK_PATH}/include)
endif (VulkanHeaders_FOUND)

find_path(OPENVR_INCLUDE_DIR openvr.h PATH_SUFFIXES openvr)
target_include_directories(${PROJECT_NAME} SYSTEM AFTER PRIVATE ${OPENVR_INCLUDE_DIR})
if (MSVC)
	target_link_directories(${PROJECT_NAME} PRIVATE ${OPENVR_INCLUDE_DIR}/../lib)
endif (MSVC)
target_link_libraries(${PROJECT_NAME} PRIVATE openvr_api)

find_package(OpenXR REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenXR::openxr_loader)
target_include_directories(${PROJECT_NAME} SYSTEM AFTER PRIVATE ${OpenXR_INCLUDE_DIR})

if (UNIX)
	target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
endif (UNIX)

## add libfloor
if (LIBFLOOR_USER)
	# add libfloor(d).so to rpath
	if (NOT BUILD_STANDALONE)
		set_target_properties(${PROJECT_NAME} PROPERTIES BUILD_RPATH "/opt/floor/lib")
	endif (NOT BUILD_STANDALONE)
	
	if (WIN32)
		target_include_directories(${PROJECT_NAME} AFTER PRIVATE $ENV{ProgramW6432}/floor/include)
	else ()
		target_include_directories(${PROJECT_NAME} AFTER PRIVATE /opt/floor/include)
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
		target_link_options(${PROJECT_NAME} PRIVATE "SHELL:-L/opt/floor/lib")
		if (CMAKE_BUILD_TYPE MATCHES "DEBUG" OR CMAKE_BUILD_TYPE MATCHES "Debug")
			target_link_options(${PROJECT_NAME} PRIVATE -lfloord)
		else ()
			target_link_options(${PROJECT_NAME} PRIVATE -lfloor)
		endif ()
	endif (UNIX)
endif (LIBFLOOR_USER)
