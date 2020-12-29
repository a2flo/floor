## libfloor dynamic library and executable CMake base configuration
# * define LIBFLOOR_USER when building a library/application using libfloor
# * define LIBFLOOR_LIBRARY when building a library using libfloor

## compile flags
target_compile_definitions(${PROJECT_NAME} PUBLIC _ENABLE_EXTENDED_ALIGNED_STORAGE)
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:DEBUG>:-DFLOOR_DEBUG>")

if (MSVC)
	set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-imsvc ")
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:DEBUG>:-gcodeview>")
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:DEBUG>:/Zi>")
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

target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:-Ofast>")
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:-msse4.1>")
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:-fstrict-aliasing>")
if (UNIX OR MINGW)
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:-ffast-math>")
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:-funroll-loops>")
endif (UNIX OR MINGW)
if (MSVC)
	target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RELEASE>:/fp:fast>")
endif (MSVC)

## warnings
# let's start with everything
target_compile_options(${PROJECT_NAME} PUBLIC -Weverything)
# in case we're using warning options that aren't supported by other clang versions
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-unknown-warning-option)
# remove std compat warnings (c++17 with gnu and clang extensions is required)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c++98-compat -Wno-c++98-compat-pedantic)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c++11-compat -Wno-c++11-compat-pedantic)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c++14-compat -Wno-c++14-compat-pedantic)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c++17-compat -Wno-c++17-compat-pedantic -Wno-c++17-extensions)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-c99-extensions -Wno-c11-extensions)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-gnu -Wno-gcc-compat)
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-nullability-extension)
# don't be too pedantic
target_compile_options(${PROJECT_NAME} PUBLIC -Wno-header-hygiene -Wno-documentation -Wno-documentation-unknown-command
	-Wno-old-style-cast -Wno-global-constructors -Wno-exit-time-destructors -Wno-reserved-id-macro -Wno-date-time)
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

## use console subsystem (MSVC)
if (MSVC)
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
	target_compile_definitions(_CONSOLE)
endif (MSVC)

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

if (MSVC)
	target_link_directories(${PROJECT_NAME} PRIVATE $ENV{VK_SDK_PATH}/Lib)
	target_link_libraries(${PROJECT_NAME} PRIVATE vulkan-1)
endif (MSVC)
if (MINGW)
	target_link_options(${PROJECT_NAME} PRIVATE "$ENV{VK_SDK_PATH}/Lib/vulkan-1.lib")
endif (MINGW)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE $ENV{VK_SDK_PATH}/include)

find_path(OPENVR_INCLUDE_DIR openvr.h)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${OPENVR_INCLUDE_DIR})
if (MSVC)
	target_link_directories(${PROJECT_NAME} PRIVATE ${OPENVR_INCLUDE_DIR}/../lib)
endif (MSVC)
target_link_libraries(${PROJECT_NAME} PRIVATE openvr_api)

## add libfloor
if (LIBFLOOR_USER)
	target_include_directories(${PROJECT_NAME} PRIVATE $ENV{ProgramW6432}/floor/include)
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
endif (LIBFLOOR_USER)
