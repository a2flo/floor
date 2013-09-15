
-- vars
local win_unixenv = false
local cygwin = false
local mingw = false
local clang_libcxx = false
local gcc_compat = false
local cuda = false
local pocl = false
local windows_no_cmd = false
local platform = "x32"
local system_includes = ""

-- this function returns the first result of "find basepath -name filename", this is needed on some platforms to determine the include path of a library
function find_include(filename, base_path)
	if(os.is("windows") and not win_unixenv) then
		return ""
	end
	
	local proc = io.popen("find "..base_path.." -name \""..filename.."\"", "r")
	local path_names = proc:read("*a")
	proc:close()
	
	if(string.len(path_names) == 0) then
		return ""
	end
	
	local newline = string.find(path_names, "\n")
	if newline == nil then
		return ""
	end
	
	return string.sub(path_names, 0, newline-1)
end

function add_include(path)
	system_includes = system_includes.." -isystem "..path
end


-------------------------------------------------------------------------------
-- basic config for all projects
solution "floor"
	-- scan args
	local argc = 1
	while(_ARGS[argc] ~= nil) do
		if(_ARGS[argc] == "--env") then
			argc=argc+1
			-- check if we are building with cygwin/mingw
			if(_ARGS[argc] ~= nil and _ARGS[argc] == "cygwin") then
				cygwin = true
				win_unixenv = true
			end
			if(_ARGS[argc] ~= nil and _ARGS[argc] == "mingw") then
				mingw = true
				win_unixenv = true
			end
		end
		if(_ARGS[argc] == "--clang") then
			clang_libcxx = true
		end
		if(_ARGS[argc] == "--gcc") then
			gcc_compat = true
		end
		if(_ARGS[argc] == "--platform") then
			argc=argc+1
			if(_ARGS[argc] ~= nil) then
				platform = _ARGS[argc]
			end
		end
		if(_ARGS[argc] == "--cuda") then
			cuda = true
		end
		if(_ARGS[argc] == "--pocl") then
			pocl = true
		end
		if(_ARGS[argc] == "--windows") then
			windows_no_cmd = true
		end
		if(_ARGS[argc] == "--cl-profiling") then
			defines { "FLOOR_PROFILING=1" }
		end
		argc=argc+1
	end
	defines { "TCC_LIB_ONLY=1" }
	if(pocl) then
		defines { "FLOOR_POCL" }
	end
	
	configurations { "Release", "Debug" }
	
	-- os specifics
	if(not os.is("windows") or win_unixenv) then
		if(not cygwin) then
			add_include("/usr/include")
		else
			add_include("/usr/include/w32api")
			add_include("/usr/include/w32api/GL")
		end
		add_include("/usr/local/include")
		add_include("/usr/include/libxml2")
		add_include("/usr/include/libxml")
		add_include("/usr/local/include/libxml2")
		add_include("/usr/include/freetype2")
		add_include("/usr/local/include/freetype2")
		buildoptions { "-std=c++11 -Wall" }
		
		if(clang_libcxx) then
			buildoptions { "-stdlib=libc++" }
			buildoptions { "-Weverything" }
			buildoptions { "-Wno-unknown-warning-option" }
			buildoptions { "-Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-header-hygiene -Wno-gnu -Wno-float-equal" }
			buildoptions { "-Wno-documentation -Wno-system-headers -Wno-global-constructors -Wno-padded -Wno-packed" }
			buildoptions { "-Wno-switch-enum -Wno-sign-conversion -Wno-conversion -Wno-exit-time-destructors -Wno-nested-anon-types" }
			linkoptions { "-fvisibility=default" }
			if(not win_unixenv) then
				buildoptions { "-integrated-as" }
				defines { "FLOOR_EXPORT=1" }
				linkoptions { "-stdlib=libc++" }
				if(os.is("linux")) then
					linkoptions { "-lc++abi" }
				end
			else
				-- "--allow-multiple-definition" is necessary, because gcc is still used as a linker
				-- and will always link against libstdc++ (-> multiple definitions with libc++)
				-- also note: since libc++ is linked first, libc++'s functions will be used
				linkoptions { "-stdlib=libc++ -lc++.dll -Wl,--allow-multiple-definition" }
			end
		end
		
		if(gcc_compat) then
			buildoptions { "-Wno-trigraphs -Wreturn-type -Wunused-variable -Wno-strict-aliasing" }
		end
		
		if(cuda) then
			add_include("/usr/local/cuda/include")
			add_include("/usr/local/cuda-5.5/include")
			add_include("/opt/cuda/include")
			add_include("/opt/cuda-5.5/include")
			defines { "FLOOR_CUDA_CL=1" }
			if(platform == "x64") then
				libdirs { "/opt/cuda-toolkit/lib64", "/usr/local/cuda/lib64", "/usr/local/cuda-5.5/lib64", "/opt/cuda/lib64", "/opt/cuda-5.5/lib64" }
			else
				libdirs { "/opt/cuda-toolkit/lib", "/usr/local/cuda/lib", "/usr/local/cuda-5.5/lib", "/opt/cuda/lib", "/opt/cuda-5.5/lib" }
			end
			links { "cuda", "cudart" }
		end
	end
	
	if(win_unixenv) then
		defines { "WIN_UNIXENV" }
		if(cygwin) then
			defines { "CYGWIN" }
		end
		if(mingw) then
			defines { "__WINDOWS__", "MINGW" }
			add_include("/mingw/include")
			libdirs { "/usr/lib", "/usr/local/lib" }
			buildoptions { "-Wno-unknown-pragmas" }
		end
	end
	
	if(os.is("linux") or os.is("bsd") or win_unixenv) then
		add_include("/usr/include/SDL2")
		add_include("/usr/local/include/SDL2")
		-- set system includes
		buildoptions { system_includes }
		
		if(not pocl) then
			links { "OpenCL" }
		else
			links { "pocl" }
		end
		libdirs { os.findlib("GL"), os.findlib("xml2"), os.findlib("OpenCL") }
		if(not win_unixenv) then
			links { "GL", "SDL2_image", "Xxf86vm", "xml2" }
			libdirs { os.findlib("SDL2"), os.findlib("SDL2_image"), os.findlib("Xxf86vm") }
			buildoptions { "`sdl2-config --cflags`" }
			linkoptions { "`sdl2-config --libs`" }
		elseif(cygwin) then
			-- link against windows opengl libs on cygwin
			links { "opengl32", "SDL2_image.dll", "xml2" }
			libdirs { "/lib/w32api" }
			buildoptions { "`sdl2-config --cflags | sed -E 's/-Dmain=SDL_main//g'`" }
			linkoptions { "`sdl2-config --libs | sed -E 's/(-lmingw32|-mwindows)//g'`" }
		elseif(mingw) then
			-- link against windows opengl libs on mingw
			links { "opengl32", "glu32", "gdi32", "SDL2_image", "libxml2", "pthread" }
			buildoptions { "`sdl2-config --cflags | sed -E 's/-Dmain=SDL_main//g'`" }
			if(windows_no_cmd) then
				linkoptions { "`sdl2-config --libs`" }
			else
				-- no -mwindows flag -> creates a separate cmd window + working iostream output
				linkoptions { "`sdl2-config --libs | sed -E 's/-mwindows//g'`" }
			end
		end

		if(gcc_compat) then
			if(not mingw) then
				defines { "_GLIBCXX__PTHREADS" }
			end
			defines { "_GLIBCXX_USE_NANOSLEEP", "_GLIBCXX_USE_SCHED_YIELD" }
		end
	end

	-- prefer system platform
	if(platform == "x64") then
		platforms { "x64", "x32" }
	else
		platforms { "x32", "x64" }
	end
	
	configuration { "x64" }
		defines { "PLATFORM_X64" }

	configuration { "x32" }
		defines { "PLATFORM_X86" }

-------------------------------------------------------------------------------
-- floor base lib
project "libfloor"
	-- project settings
	targetname "libfloor"
	kind "SharedLib"
	language "C++"

	files { "**.h", "**.hpp", "**.cpp", "**.cc", "**.c" }
	basedir "."
	targetdir "bin"
	includedirs { ".",
				  "cl/",
				  "core/",
				  "cuda/",
				  "floor/",
				  "hash/",
				  "net/",
				  "tccpp/",
				  "threading/" }
	
	if(not os.is("windows") or win_unixenv) then
		prebuildcommands { "./floor/build_version.sh" }
		if(mingw) then
			postbuildcommands { "./install.sh" }
		end
	end

	configuration "Release"
		targetname "floor"
		defines { "NDEBUG" }
		flags { "Optimize" }
		if(not os.is("windows") or win_unixenv) then
			buildoptions { "-O3 -ffast-math" }
		end

	configuration "Debug"
		targetname "floord"
		defines { "DEBUG", "FLOOR_DEBUG" }
		flags { "Symbols" }
		if(not os.is("windows") or win_unixenv) then
			buildoptions { "-gdwarf-2" }
		end
