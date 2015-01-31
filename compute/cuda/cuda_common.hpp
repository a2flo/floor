/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#ifndef __FLOOR_CUDA_COMMON_HPP__
#define __FLOOR_CUDA_COMMON_HPP__

#include <floor/core/essentials.hpp>

// need at least cuda 6.5 to compile and run
#define FLOOR_CUDA_API_VERSION_MIN 6050

#if !defined(FLOOR_NO_CUDA)

#include <floor/core/gl_support.hpp>

#if defined(__APPLE__)
#include <CUDA/cuda.h>
#include <CUDA/cudaGL.h>
#else
#include <cuda.h>
#include <cudaGL.h>
#endif

//
#define CU_CALL_FWD(call, error_msg, line_num, do_stuff) {									\
	CUresult _cu_err = call;																\
	/* check if call was successful, or if cuda is already shutting down, */				\
	/* in which case we just pretend nothing happened and continue ...    */				\
	if(_cu_err != CUDA_SUCCESS && _cu_err != CUDA_ERROR_DEINITIALIZED) {					\
		const char* err_name, *err_str;														\
		cuGetErrorName(_cu_err, &err_name);													\
		cuGetErrorString(_cu_err, &err_str);												\
		log_error("%s: line %u: cuda error %s (#%u): %s (call: %s)",						\
				  error_msg, line_num, (err_name != nullptr ? err_name : "INVALID"),		\
				  _cu_err, (err_str != nullptr ? err_str : "INVALID"), #call);				\
		do_stuff																			\
	}																						\
}
#define CU_CALL_RET(call, error_msg, ...) CU_CALL_FWD(call, error_msg, __LINE__, return __VA_ARGS__ ;)
#define CU_CALL_CONT(call, error_msg, ...) CU_CALL_FWD(call, error_msg, __LINE__, continue;)
#define CU_CALL_IGNORE(call, ...) CU_CALL_FWD(call, "cuda error", __LINE__,)

#endif

#endif
