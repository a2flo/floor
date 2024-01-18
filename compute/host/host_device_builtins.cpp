/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#include <floor/compute/host/host_device_builtins.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)
#include <floor/compute/host/host_common.hpp>
#include <cmath>

#define FLOOR_HOST_DEVICE_FUNC_PREFIX_GET() FLOOR_HOST_DEVICE_FUNC_PREFIX
#define FLOOR_HOST_DEVICE_FUNC_PREFIX_STR_STRINGIFY(str) #str
#define FLOOR_HOST_DEVICE_FUNC_PREFIX_STR_EVAL(str) FLOOR_HOST_DEVICE_FUNC_PREFIX_STR_STRINGIFY(str)
#define FLOOR_HOST_DEVICE_FUNC_PREFIX_STR FLOOR_HOST_DEVICE_FUNC_PREFIX_STR_EVAL(FLOOR_HOST_DEVICE_FUNC_PREFIX_GET())

#define FLOOR_HOST_DEVICE_BUILTIN_CONCAT(prefix, c_func_name) prefix ## c_func_name
#define FLOOR_HOST_DEVICE_BUILTIN_CONCAT_EVAL(prefix, c_func_name) FLOOR_HOST_DEVICE_BUILTIN_CONCAT(prefix, c_func_name)
#define FLOOR_HOST_DEVICE_BUILTIN(c_func_name) FLOOR_HOST_DEVICE_BUILTIN_CONCAT_EVAL(FLOOR_HOST_DEVICE_FUNC_PREFIX, c_func_name)

#define FLOOR_HOST_DEVICE_FUNC_DECL extern "C" __attribute__((used, const, visibility("default")))

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(missing-prototypes)

// function definitions / forwarders
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(sinf)(const float val) FLOOR_HOST_COMPUTE_CC {
	return ::sinf(val);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(cosf)(const float val) FLOOR_HOST_COMPUTE_CC {
	return ::cosf(val);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(tanf)(const float val) FLOOR_HOST_COMPUTE_CC {
	return ::tanf(val);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(asinf)(const float val) FLOOR_HOST_COMPUTE_CC {
	return ::asinf(val);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(acosf)(const float val) FLOOR_HOST_COMPUTE_CC {
	return ::acosf(val);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(atanf)(const float val) FLOOR_HOST_COMPUTE_CC {
	return ::atanf(val);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(atan2f)(const float a, const float b) FLOOR_HOST_COMPUTE_CC {
	return ::atan2f(a, b);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(sinhf)(const float val) FLOOR_HOST_COMPUTE_CC {
	return ::sinhf(val);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(coshf)(const float val) FLOOR_HOST_COMPUTE_CC {
	return ::coshf(val);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(tanhf)(const float val) FLOOR_HOST_COMPUTE_CC {
	return ::tanhf(val);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(asinhf)(const float val) FLOOR_HOST_COMPUTE_CC {
	return ::asinhf(val);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(acoshf)(const float val) FLOOR_HOST_COMPUTE_CC {
	return ::acoshf(val);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(atanhf)(const float val) FLOOR_HOST_COMPUTE_CC {
	return ::atanhf(val);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(expf)(const float val) FLOOR_HOST_COMPUTE_CC {
	return ::expf(val);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(exp2f)(const float val) FLOOR_HOST_COMPUTE_CC {
	return ::exp2f(val);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(logf)(const float val) FLOOR_HOST_COMPUTE_CC {
	return ::logf(val);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(log2f)(const float val) FLOOR_HOST_COMPUTE_CC {
	return ::log2f(val);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(powf)(const float a, const float b) FLOOR_HOST_COMPUTE_CC {
	return ::powf(a, b);
}
FLOOR_HOST_DEVICE_FUNC_DECL float FLOOR_HOST_DEVICE_BUILTIN(fmodf)(const float a, const float b) FLOOR_HOST_COMPUTE_CC {
	return ::fmodf(a, b);
}

FLOOR_POP_WARNINGS()

//
const std::unordered_map<std::string, std::string>& floor_get_c_to_floor_builtin_map() {
#define FLOOR_FUNC_ENTRY(c_func_name) { #c_func_name, FLOOR_HOST_DEVICE_FUNC_PREFIX_STR #c_func_name },
	static const std::unordered_map<std::string, std::string> func_map {
		FLOOR_FUNC_ENTRY(sinf)
		FLOOR_FUNC_ENTRY(cosf)
		FLOOR_FUNC_ENTRY(tanf)
		FLOOR_FUNC_ENTRY(asinf)
		FLOOR_FUNC_ENTRY(acosf)
		FLOOR_FUNC_ENTRY(atanf)
		FLOOR_FUNC_ENTRY(atan2f)
		FLOOR_FUNC_ENTRY(sinhf)
		FLOOR_FUNC_ENTRY(coshf)
		FLOOR_FUNC_ENTRY(tanhf)
		FLOOR_FUNC_ENTRY(asinhf)
		FLOOR_FUNC_ENTRY(acoshf)
		FLOOR_FUNC_ENTRY(atanhf)
		FLOOR_FUNC_ENTRY(expf)
		FLOOR_FUNC_ENTRY(exp2f)
		FLOOR_FUNC_ENTRY(logf)
		FLOOR_FUNC_ENTRY(log2f)
		FLOOR_FUNC_ENTRY(powf)
		FLOOR_FUNC_ENTRY(fmodf)
	};
#undef FLOOR_FUNC_ENTRY
	return func_map;
}

#endif
